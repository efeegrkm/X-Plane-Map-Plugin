// MapData.cpp
#include "engine/MapData.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <vector>

#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#include "Log.h"
#include "MapTheme.h"
#include "MapWindow.h"
#include "data/AptDatIndexer.h"
#include "data/SceneryIndex.h"
#include "data/TerrainPaletteSource.h"
#include "layers/AircraftLayer.h"
#include "layers/AirportLayer.h"
#include "layers/AirportObjectsLayer.h"
#include "layers/AirportSurfaceLayer.h"
#include "layers/AirwayLayer.h"
#include "layers/DrawingLayer.h"
#include "layers/FmsLayer.h"
#include "layers/GridLayer.h"
#include "layers/NavaidLayer.h"
#include "layers/RoadLayer.h"
#include "layers/RunwayLayer.h"
#include "layers/TaxiRouteLayer.h"
#include "layers/TerrainLayer.h"
#include "layers/TrackLayer.h"

namespace
{

const char* kTerrainCacheFolder = "terrain/";
const char* kAirportCacheFolder = "airports/";
const float kPrefetchPeriod = 5.0f;                 // onceden yukleme listesi bu kadar saniyede bir yenilenir
const double kTileBytesEstimate = 3.5 * 1048576.0;  // karo basina onbellek (raster + rakim + vektor)
const double kPrecacheLimitShare = 0.8;  // bolge onbellegi, onbellek sinirinin en cok bu kadarini doldurabilir
const float kNoticeSeconds = 8.0f;       // alt seritteki kisa bildirimlerin suresi
const double kNavLoadBudgetMs = 4.0;     // nav verisi okunurken her karede en fazla bu kadar

std::string SystemPath()
{
    std::array<char, 2048> tSystemPath = {};
    XPLMGetSystemPath(tSystemPath.data());  // "C:\X-Plane 11\" (sonunda ayirici ile)
    return tSystemPath.data();
}

void AddPart(std::string* pMessage, const std::string& pPart)
{
    if (pPart.empty())
    {
        return;
    }
    *pMessage += pMessage->empty() ? "" : "   ";
    *pMessage += pPart;
}

}  // namespace

MapData::MapData(const MapDataConfig& pConfig)
    : mConfig(pConfig)
    , mAirportGeometry(std::make_unique<AirportGeometryStore>(std::make_unique<AptDatIndexer>()))
{
    mAirportDetails = std::make_unique<AirportDetailStore>(*mAirportGeometry);
}

MapData::~MapData()
{
    Stop();
}

void MapData::Start()
{
    mXPlaneDirectory = SystemPath();
    mAircraft.Init();
    ApplyXPlaneColors();
    CreateLayers();
    XPLMRegisterFlightLoopCallback(FlightLoopCallback, -1.0f, this);
    mFlightLoopRegistered = true;
}

void MapData::Stop()
{
    if (mFlightLoopRegistered)
    {
        XPLMUnregisterFlightLoopCallback(FlightLoopCallback, this);
        mFlightLoopRegistered = false;
    }
    mTerrainStore.Stop();  // zemin iscisi: o an uretilen karoyu bitirip durur
    mAirportDetails->Stop();
}

void MapData::AttachWindow(MapWindow* pWindow)
{
    mWindow = pWindow;
}

LayerManager& MapData::Layers()
{
    return mLayers;
}

Drawings& MapData::UserDrawings()
{
    return mDrawings;
}

AircraftData& MapData::Aircraft()
{
    return mAircraft;
}

FmsLayer* MapData::Fms()
{
    return mFmsLayer;
}

TrackLayer* MapData::Track()
{
    return mTrackLayer;
}

const AirportGeometryStore& MapData::Airports() const
{
    return *mAirportGeometry;
}

const MapDataStatus& MapData::Status() const
{
    return mStatus;
}

const TerrainPalette& MapData::Palette() const
{
    return mTerrainPalette;
}

void MapData::CreateLayers()
{
    // Alttan uste cizim sirasi (ayrinti: MIMARI.pdf bolum 7). Etiketler de bu sirayla
    // yer bulur: ustteki satirdaki katmanin etiketi onceliklidir (FMS ve
    // kullanici cizimleri etiketlerinin yerini onceden ayirir).
    std::unique_ptr<TerrainLayer> tTerrain = std::make_unique<TerrainLayer>(mTerrainStore, mTerrainPalette);
    mTerrainLayer = tTerrain.get();
    mLayers.Add(std::move(tTerrain));  // en altta: zemin
    mLayers.Add(std::make_unique<RoadLayer>(mTerrainStore));  // DSF yollari: zeminin ustu, yuzeylerin alti
    mLayers.Add(std::make_unique<AirportSurfaceLayer>(*mAirportGeometry, *mAirportDetails));
    mLayers.Add(std::make_unique<GridLayer>());
    // ILS tuyleri pistlerin ALTINDA: buyuk yari saydam alanlar pistleri ortmesin
    mLayers.Add(std::make_unique<NavaidLayer>(NavaidType::Ils, mNavDatabase, mAirportGeometry.get()));
    mLayers.Add(std::make_unique<RunwayLayer>(*mAirportGeometry));
    mLayers.Add(std::make_unique<TaxiRouteLayer>(*mAirportGeometry, *mAirportDetails));  // varsayilan kapali
    mLayers.Add(std::make_unique<AirportObjectsLayer>(*mAirportGeometry, *mAirportDetails));  // pistlerin ustunde
    // Hava yollari navaid sembollerinin altinda (cizgiler sembolleri ortmesin)
    mLayers.Add(std::make_unique<AirwayLayer>(mAirways, false));
    mLayers.Add(std::make_unique<AirwayLayer>(mAirways, true));
    mLayers.Add(std::make_unique<AirportLayer>(mNavDatabase, mAirportGeometry.get()));
    mLayers.Add(std::make_unique<NavaidLayer>(NavaidType::Vor, mNavDatabase));
    mLayers.Add(std::make_unique<NavaidLayer>(NavaidType::Ndb, mNavDatabase));
    mLayers.Add(std::make_unique<NavaidLayer>(NavaidType::Fix, mNavDatabase));
    std::unique_ptr<FmsLayer> tFms = std::make_unique<FmsLayer>();
    mFmsLayer = tFms.get();
    mLayers.Add(std::move(tFms));
    std::unique_ptr<TrackLayer> tTrack = std::make_unique<TrackLayer>(mAircraft);
    mTrackLayer = tTrack.get();
    mLayers.Add(std::move(tTrack));
    mLayers.Add(std::make_unique<AircraftLayer>(mAircraft));
    mLayers.Add(std::make_unique<DrawingLayer>(mDrawings));
}

// --- Zemin ayarlari --------------------------------------------------------------

void MapData::ImportTerrainSettings(const std::string& pText)
{
    Settings tColors;
    tColors.LoadText(pText);
    mTerrainPalette.Load(tColors);
    ApplyXPlaneColors();
    if (mLoadStarted)
    {
        Log("terrain settings changed after start: applied at the next X-Plane start");
    }
}

std::string MapData::ExportTerrainSettings() const
{
    const char* kComment = "Prime Map terrain colors. Edit while X-Plane is closed.\n"
                           "[colors]: <class> = R G B (0..255). [shading] strength = 0 (flat) .. 1 (strong relief).\n"
                           "[blend] meters = soft transition between land classes in Scenery mode "
                           "(0 = off, default 150).\n"
                           "[blend] max_pixels = cap of the blend radius in texture pixels at close zoom "
                           "(default 5; 0 = no cap).\n"
                           "[warp] meters / wavelength = natural (curved) borders between land classes; "
                           "meters 0 = straight triangle edges (default 65 / 250).\n"
                           "[closeup] class_mix = 0..0.8: at close zoom land class colors move this much toward one "
                           "land tone (0 = off, default 0.3).\n"
                           "[relief] multi_directional = 1 (light from 4 directions, mostly northwest) / 0 (one "
                           "light); exaggeration (2.5); coarse_boost (0.35: stronger relief when zoomed out); "
                           "slope_tone (0.12: darker steep slopes); flat_slope (0.02: slopes below this fade out, "
                           "hides 1 m elevation steps on flat land; 0 = off).\n"
                           "[water] coastline = 1/0, coastline_strength (0.5); band_meters (200: light band along the "
                           "coast, 0 = off), band_max_pixels (24), band_strength (0.10); depth_tone (0.25: darker "
                           "deep sea from the DSF sea_level data, 0 = off), depth_full_meters (200).\n"
                           "[vfr] bands = 1 (sectional elevation bands) / 0 (smooth ramp); contour_lines = 0/1, "
                           "contour_strength (0.35); band_1 .. band_8 = <lower limit feet> R G B.\n"
                           "[palette] source = xplane (land class colors averaged from X-Plane's own terrain "
                           "textures) / ini (use the [colors] values below); version: do not edit.\n"
                           "Colors are applied when X-Plane starts; the cache/terrain folder does not need to be "
                           "deleted.\n"
                           "Delete this file to get the default colors back.";
    Settings tColors;
    mTerrainPalette.Save(tColors);
    tColors.SetComment(kComment);
    return tColors.ToText({});
}

void MapData::ApplyXPlaneColors()
{
    // Asama 3: sinif renkleri X-Plane'in arazi dokularindan (source = xplane); ilk acilistan sonra onbellekten
    if (!mTerrainPalette.UsesXPlaneColors())
    {
        return;
    }
    XPlaneClassColors tDerived;
    std::string tReport;
    if (LoadXPlaneClassColors(SystemPath(), mConfig.mCacheDirectory + kTerrainCacheFolder + "xplane_palette.txt",
                              &tDerived, &tReport))
    {
        mTerrainPalette.ApplyDerivedColors(tDerived.mColors, tDerived.mTextureCounts);
    }
    Log("{}", tReport);
}

// --- Flight loop -------------------------------------------------------------------

float MapData::FlightLoopCallback(float /*pSinceLastCall*/, float /*pSinceLastLoop*/, int /*pCounter*/,
                                  void* pRefcon)
{
    return static_cast<MapData*>(pRefcon)->OnFlightLoop();
}

float MapData::OnFlightLoop()
{
    const float tNow = XPLMGetElapsedTime();
    StepLoading();
    mAircraft.Refresh();
    mLayers.UpdateAll(tNow);
    UpdatePrefetch(tNow);
    UpdateStatus();
    if (mWindow)
    {
        mWindow->Update(tNow);
    }
    return -1.0f;  // her karede tekrar cagir
}

void MapData::StepLoading()
{
    // Ilk flight loop'ta (X-Plane hazir) arka plan islerini baslat
    if (!mLoadStarted)
    {
        mLoadStarted = true;
        Log("indexing airports in the background: {}", mAirportGeometry->SourceName());
        mAirportGeometry->StartLoading(mXPlaneDirectory, mConfig.mCacheDirectory + kAirportCacheFolder);
        mTerrainStore.SetCacheLimit(static_cast<uint64_t>(std::max(0, mConfig.mTerrainCacheMb)) << 20);
        mTerrainStore.Start(mXPlaneDirectory, mConfig.mCacheDirectory + kTerrainCacheFolder, mTerrainPalette,
                            AllTerrainStyles());
    }
    LogReport(mTerrainStore.TakeLogLines());
    mAirportDetails->Update();  // onceki karede istenen havalimani ayrintilari isciye
    LogReport(mAirportDetails->TakeLogLines());
    if (mAirportGeometry->Poll())
    {
        Log("airport index {}: {} airports, {} runways", mAirportGeometry->LastLoadSucceeded() ? "ready" : "FAILED",
            mAirportGeometry->Count(), mAirportGeometry->RunwayCount());
        LogReport(mAirportGeometry->LastReport());
        mAirportDetails->Start();  // ayrintilar artik dosyadaki yerlerinden okunabilir
    }

    // Nav verisi: ana thread'de, her karede birkac ms
    if (!mNavLoader.IsDone())
    {
        if (mNavLoader.Step(kNavLoadBudgetMs))
        {
            // Hava yollarinin uc noktalari nav verisinden bulunur: nav bitince basla.
            // X-Plane'in kurali: once Custom Data (guncel AIRAC), yoksa varsayilan.
            mAirways.StartLoading({mXPlaneDirectory + "Custom Data/earth_awy.dat",
                                   mXPlaneDirectory + "Resources/default data/earth_awy.dat"},
                                  mNavDatabase);
        }
    }
    if (mAirways.Poll())
    {
        Log("airways {}: {}", mAirways.IsReady() ? "ready" : "FAILED", mAirways.LastReport());
    }
}

void MapData::UpdateStatus()
{
    MapDataStatus tStatus;
    tStatus.mNavReady = mNavLoader.IsDone();
    tStatus.mAirportsReady = !mAirportGeometry->IsLoading() && mAirportGeometry->Count() > 0;
    tStatus.mAirwaysReady = mAirways.IsReady();
    tStatus.mTerrainIndexReady = mTerrainStore.IsIndexReady();

    std::string tMessage;
    if (!mNavLoader.IsDone() && mNavLoader.VisitedCount() > 0)
    {
        AddPart(&tMessage, std::format("Loading nav data: {}", mNavLoader.VisitedCount()));
    }
    if (mAirportGeometry->IsLoading())
    {
        AddPart(&tMessage, "Indexing airports...");
    }
    if (mAirportDetails->PendingCount() > 0)
    {
        AddPart(&tMessage, std::format("Loading airport details: {}", mAirportDetails->PendingCount()));
    }
    if (mAirways.IsLoading())
    {
        AddPart(&tMessage, "Reading airways...");
    }
    // Zemin: ekranda istenen kalitede olmayan (kaba yedek / bos) doku oldugu surece yaz.
    // Isci bosta ise bekleyen bir sey yoktur (uretilemeyen parca): yazma, takili kalmasin.
    std::string tTerrainStatus = mTerrainStore.StatusText();
    const int tPending = (mTerrainLayer && mTerrainLayer->IsVisible() && mWindow && mWindow->IsVisible())
                             ? mTerrainLayer->PendingTextureCount()
                             : 0;
    if (tPending > 0 && !mTerrainStore.IsIdle())
    {
        tStatus.mTerrainPendingImages = tPending;
        const std::string tLoading = std::format("Loading terrain: {} image{}", tPending, tPending == 1 ? "" : "s");
        tTerrainStatus = tTerrainStatus.empty() ? tLoading : tLoading + " (" + tTerrainStatus + ")";
    }
    AddPart(&tMessage, tTerrainStatus);
    // Bolge onbellegi ilerlemesi ve bitisi
    const bool tPrecaching = mTerrainStore.PrecacheProgress(&tStatus.mPrecacheDone, &tStatus.mPrecacheTotal);
    if (tPrecaching)
    {
        AddPart(&tMessage, std::format("Pre-caching terrain: {}/{}", tStatus.mPrecacheDone, tStatus.mPrecacheTotal));
    }
    else if (mPrecacheRunning)
    {
        Log("terrain pre-cache finished: {} tiles", tStatus.mPrecacheDone);
        ShowNotice(std::format("Terrain pre-cache finished: {} tiles", tStatus.mPrecacheDone));
    }
    mPrecacheRunning = tPrecaching;
    tStatus.mPrecacheRunning = tPrecaching;
    if (!mNotice.empty() && XPLMGetElapsedTime() < mNoticeUntil)
    {
        AddPart(&tMessage, mNotice);
    }
    tStatus.mText = tMessage;
    mStatus = tStatus;

    if (mWindow)
    {
        mWindow->SetStatusMessage(tMessage);
        // Harita resmi onbellegi: veri gelirken resim hiz sinirinda yenilensin (MapImageCache)
        const bool tBusy = !mNavLoader.IsDone() || mAirportGeometry->IsLoading() ||
                           mAirportDetails->PendingCount() > 0 || mAirways.IsLoading() || !mTerrainStore.IsIdle();
        mWindow->SetDataBusy(tBusy);
    }
}

void MapData::ShowNotice(const std::string& pText)
{
    mNotice = pText;
    mNoticeUntil = XPLMGetElapsedTime() + kNoticeSeconds;
}

void MapData::UpdatePrefetch(float pNow)
{
    if (pNow - mLastPrefetch < kPrefetchPeriod || mConfig.mPrefetchTiles <= 0 || !mTerrainStore.IsIndexReady())
    {
        return;
    }
    mLastPrefetch = pNow;
    std::vector<int> tTiles;
    auto tAdd = [this, &tTiles](const LatLon& pPoint)
    {
        const int tWest = static_cast<int>(std::floor(pPoint.mLon));
        const int tSouth = static_cast<int>(std::floor(pPoint.mLat));
        if (static_cast<int>(tTiles.size()) >= mConfig.mPrefetchTiles || tSouth < -90 || tSouth > 89 ||
            mTerrainStore.CoverageOf(tWest, tSouth) != TileCoverage::HasDsf)
        {
            return;
        }
        const int tKey = SceneryIndex::TileKey(tWest, tSouth);
        if (std::find(tTiles.begin(), tTiles.end(), tKey) == tTiles.end())
        {
            tTiles.push_back(tKey);
        }
    };
    // Ucagin onu: hareket ediyorsa gidis yonunde 120 NM'ye kadar
    const AircraftState& tAircraft = mAircraft.State();
    LatLon tFrom = tAircraft.mPosition;
    if (tAircraft.mValid)
    {
        tAdd(tAircraft.mPosition);
        if (tAircraft.mGroundSpeedKt > 40.0)
        {
            for (double tDistance = 15.0; tDistance <= 120.0; tDistance += 15.0)
            {
                tAdd(DestinationPoint(tAircraft.mPosition, tAircraft.mHeadingDeg, tDistance));
            }
        }
    }
    // FMS rotasinin kalan kismi: bacaklar ~10 NM aralikla orneklenir
    const std::vector<LatLon> tRoute = mFmsLayer ? mFmsLayer->RouteAhead() : std::vector<LatLon>();
    for (size_t tIndex = 0; tIndex < tRoute.size(); ++tIndex)
    {
        if (tIndex == 0 && !tAircraft.mValid)
        {
            tFrom = tRoute[0];
        }
        const double tLength = DistanceNm(tFrom, tRoute[tIndex]);
        const double tBearing = InitialBearingDeg(tFrom, tRoute[tIndex]);
        for (double tDistance = 0.0; tDistance < tLength; tDistance += 10.0)
        {
            tAdd(DestinationPoint(tFrom, tBearing, tDistance));
        }
        tAdd(tRoute[tIndex]);
        tFrom = tRoute[tIndex];
    }
    mTerrainStore.SetPrefetchTiles(tTiles);
}

bool MapData::StartPrecache(const GeoBounds& pBounds)
{
    if (!mTerrainStore.IsIndexReady())
    {
        ShowNotice("Pre-cache: scenery index is not ready yet");
        return false;
    }
    const int tSouth = std::max(-90, static_cast<int>(std::floor(pBounds.mSouth)));
    const int tNorth = std::min(89, static_cast<int>(std::floor(pBounds.mNorth)));
    const int tWest = static_cast<int>(std::floor(pBounds.mWest));
    const int tEast = std::min(tWest + 359, static_cast<int>(std::floor(pBounds.mEast)));
    std::vector<int> tTiles;
    for (int tLat = tSouth; tLat <= tNorth; ++tLat)
    {
        for (int tColumn = tWest; tColumn <= tEast; ++tColumn)
        {
            const int tLon = ((tColumn + 180) % 360 + 360) % 360 - 180;
            if (mTerrainStore.CoverageOf(tLon, tLat) == TileCoverage::HasDsf)
            {
                tTiles.push_back(SceneryIndex::TileKey(tLon, tLat));
            }
        }
    }
    if (tTiles.empty())
    {
        ShowNotice("Pre-cache: no installed scenery in the visible area");
        return false;
    }
    // Onbellek siniriyla catismasin: sigmayacaksa hic baslama (CacheTrim yeni karolari silerdi)
    const double tEstimateMb = static_cast<double>(tTiles.size()) * kTileBytesEstimate / 1048576.0;
    const int tLimitMb = mConfig.mTerrainCacheMb;
    if (tLimitMb > 0 && tEstimateMb > tLimitMb * kPrecacheLimitShare)
    {
        ShowNotice(std::format("Pre-cache: area too large ({} tiles, ~{:.0f} MB; cache limit {} MB). Zoom in "
                               "or raise [cache] terrain_mb",
                               tTiles.size(), tEstimateMb, tLimitMb));
        return false;
    }
    mTerrainStore.StartPrecache(tTiles);
    mPrecacheRunning = true;
    Log("terrain pre-cache started: {} tiles (~{:.0f} MB)", tTiles.size(), tEstimateMb);
    return true;
}

void MapData::CancelPrecache()
{
    if (mPrecacheRunning)
    {
        mTerrainStore.CancelPrecache();
        ShowNotice("Terrain pre-cache cancelled");
    }
}

void MapData::LogReport(const std::string& pReport)
{
    // Rapor birden cok satir olabilir: her satiri ayri log satiri yap
    size_t tStart = 0;
    while (tStart < pReport.size())
    {
        size_t tEnd = pReport.find('\n', tStart);
        if (tEnd == std::string::npos)
        {
            tEnd = pReport.size();
        }
        if (tEnd > tStart)
        {
            Log("  {}", pReport.substr(tStart, tEnd - tStart));
        }
        tStart = tEnd + 1;
    }
}
