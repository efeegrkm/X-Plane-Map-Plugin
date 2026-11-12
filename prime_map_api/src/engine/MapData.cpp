// MapData.cpp
#include "engine/MapData.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <format>
#include <system_error>
#include <vector>

#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#include "Log.h"
#include "MapTheme.h"
#include "data/AptDatIndexer.h"
#include "data/SceneryIndex.h"
#include "data/TerrainPaletteSource.h"
#include "engine/MapInstance.h"
#include "layers/FmsLayer.h"

namespace
{

const char* kTerrainCacheFolder = "terrain/";
const char* kAirportCacheFolder = "airports/";
const char* kSettingsFileName = "prime_map.ini";
const char* kMapsFolder = "maps/";
const char* kCacheSection = "cache";
const char* kMemorySection = "memory";
const char* kMapsSection = "maps";
const float kPrefetchPeriod = 5.0f;                 // onceden yukleme listesi bu kadar saniyede bir yenilenir
const double kTileBytesEstimate = 3.5 * 1048576.0;  // karo basina onbellek (raster + rakim + vektor)
const double kPrecacheLimitShare = 0.8;  // bolge onbellegi, onbellek sinirinin en cok bu kadarini doldurabilir
const float kNoticeSeconds = 8.0f;       // alt seritteki kisa bildirimlerin suresi
const double kNavLoadBudgetMs = 4.0;     // nav verisi okunurken her karede en fazla bu kadar

const char* kSettingsComment =
    "Prime Map central settings (shared by ALL maps of this X-Plane process).\n"
    "Per-map settings live in maps/<map name>/ (map_settings.ini, map_drawings.txt, terrain_colors.ini).\n"
    "The cache (cache/terrain, cache/airports) is shared by all maps; its limits grow with the number of\n"
    "OPEN maps:  limit(n) = base x (1 + growth_percent_per_map x (n - 1) / 100), capped by the max_ value.\n"
    "[cache] terrain_mb = disk limit of the terrain cache for ONE open map, MB (0 = no limit).\n"
    "[cache] growth_percent_per_map = extra disk allowance per additional open map, percent of terrain_mb.\n"
    "[cache] max_terrain_mb = ceiling of the grown disk limit, MB (0 = no ceiling).\n"
    "[cache] prefetch_tiles = tiles prepared ahead of the aircraft / along the FMS route (0 = off).\n"
    "[memory] texture_mb = video memory for terrain textures for ONE open map, MB.\n"
    "[memory] growth_percent_per_map = extra memory per additional open map, percent (textures, tiles kept\n"
    "in RAM by the terrain worker, loaded airport details).\n"
    "[memory] max_texture_mb = ceiling of the grown texture budget, MB (0 = no ceiling).\n"
    "[maps] autosave = 1: per-map files are written automatically when a setting changes; 0: only when\n"
    "the map is closed or the host calls Map::SaveSettings.\n"
    "[maps] autosave_seconds = shortest time between two automatic writes of the same map.\n"
    "Edit while X-Plane is closed, or change at run time with MapLibrary::SetSettings.";

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

// taban x (1 + artis% x (n - 1) / 100); tavan > 0 ise onu gecmez. taban 0 = sinirsiz (0 doner).
double GrownLimit(double pBase, int pGrowthPercent, double pMax, int pOpenMaps)
{
    if (pBase <= 0.0)
    {
        return 0.0;
    }
    const int tExtraMaps = std::max(0, pOpenMaps - 1);
    double tLimit = pBase * (1.0 + std::max(0, pGrowthPercent) / 100.0 * tExtraMaps);
    if (pMax > 0.0)
    {
        tLimit = std::min(tLimit, std::max(pMax, pBase));  // tavan tabandan kucukse taban gecerli
    }
    return tLimit;
}

prime::LibrarySettings Sanitized(prime::LibrarySettings pSettings)
{
    pSettings.mTerrainCacheMb = std::max(0, pSettings.mTerrainCacheMb);
    pSettings.mCacheGrowthPercentPerMap = std::clamp(pSettings.mCacheGrowthPercentPerMap, 0, 1000);
    pSettings.mTerrainCacheMaxMb = std::max(0, pSettings.mTerrainCacheMaxMb);
    pSettings.mPrefetchTiles = std::clamp(pSettings.mPrefetchTiles, 0, 500);
    pSettings.mTextureMemoryMb = std::clamp(pSettings.mTextureMemoryMb, 64, 16384);
    pSettings.mMemoryGrowthPercentPerMap = std::clamp(pSettings.mMemoryGrowthPercentPerMap, 0, 1000);
    pSettings.mTextureMemoryMaxMb = std::max(0, pSettings.mTextureMemoryMaxMb);
    pSettings.mAutoSaveSeconds = std::clamp(pSettings.mAutoSaveSeconds, 0.0, 3600.0);
    return pSettings;
}

}  // namespace

MapData::MapData(const MapDataConfig& pConfig)
    : mConfig(pConfig)
    , mSettings(Sanitized(pConfig.mDefaults))
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
    std::error_code tError;
    std::filesystem::create_directories(mConfig.mDataDirectory, tError);
    LoadLibrarySettings();
    // Palet 0: varsayilan zemin ayarlari (terrain_colors.ini'si olmayan haritalar bunu kullanir)
    RegisterPalette(&mDefaultPalette);
    ApplyLimits();
    XPLMRegisterFlightLoopCallback(FlightLoopCallback, -1.0f, this);
    mFlightLoopRegistered = true;
    mRunning = true;
}

void MapData::Stop()
{
    mRunning = false;
    if (mFlightLoopRegistered)
    {
        XPLMUnregisterFlightLoopCallback(FlightLoopCallback, this);
        mFlightLoopRegistered = false;
    }
    mTerrainStore.Stop();  // zemin iscisi: o an uretilen karoyu bitirip durur
    mAirportDetails->Stop();
}

// --- Haritalar ---------------------------------------------------------------------

void MapData::AddInstance(MapInstance* pInstance)
{
    if (pInstance && std::find(mInstances.begin(), mInstances.end(), pInstance) == mInstances.end())
    {
        mInstances.push_back(pInstance);
    }
}

void MapData::RemoveInstance(MapInstance* pInstance)
{
    mInstances.erase(std::remove(mInstances.begin(), mInstances.end(), pInstance), mInstances.end());
    ApplyLimits();
}

void MapData::OpenMapsChanged()
{
    ApplyLimits();
}

int MapData::OpenMapCount() const
{
    return static_cast<int>(std::count_if(mInstances.begin(), mInstances.end(),
                                          [](const MapInstance* pInstance)
                                          {
                                              return pInstance->IsOpen();
                                          }));
}

// --- Ortak depolar -------------------------------------------------------------------

TerrainHub& MapData::Hub()
{
    return mHub;
}

TerrainStore& MapData::Terrain()
{
    return mTerrainStore;
}

NavDatabase& MapData::Navs()
{
    return mNavDatabase;
}

AirwayDatabase& MapData::Airways()
{
    return mAirways;
}

AirportGeometryStore& MapData::AirportGeometry()
{
    return *mAirportGeometry;
}

AirportDetailStore& MapData::AirportDetails()
{
    return *mAirportDetails;
}

const AirportGeometryStore& MapData::Airports() const
{
    return *mAirportGeometry;
}

const MapDataStatus& MapData::Status() const
{
    return mStatus;
}

// --- Zemin paletleri -------------------------------------------------------------------

int MapData::RegisterPalette(TerrainPalette* pPalette)
{
    // Asama 3: sinif renkleri X-Plane'in arazi dokularindan (source = xplane); ilk seferde hesaplanir
    // (sonra onbellekten), sonuc butun haritalar icin ayni
    if (pPalette->UsesXPlaneColors())
    {
        if (!mDerivedLoaded)
        {
            mDerivedLoaded = true;
            XPlaneClassColors tDerived;
            std::string tReport;
            mDerivedOk = LoadXPlaneClassColors(SystemPath(),
                                               mConfig.mCacheDirectory + kTerrainCacheFolder + "xplane_palette.txt",
                                               &tDerived, &tReport);
            mDerivedColors = tDerived.mColors;
            mDerivedCounts = tDerived.mTextureCounts;
            Log("{}", tReport);
        }
        if (mDerivedOk)
        {
            pPalette->ApplyDerivedColors(mDerivedColors, mDerivedCounts);
        }
    }
    // Ayni icerik = ayni numara: ayni ayarli haritalar ayni dokulari paylasir
    Settings tText;
    pPalette->Save(tText);
    const std::string tKey = tText.ToText({});
    const auto tFound = mPaletteIds.find(tKey);
    if (tFound != mPaletteIds.end())
    {
        return tFound->second;
    }
    const int tId = mTerrainStore.AddPalette(*pPalette);
    mPaletteIds.emplace(tKey, tId);
    return tId;
}

const char* MapData::TerrainSettingsComment()
{
    return "Prime Map terrain colors and terrain settings of THIS map (each map has its own file).\n"
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
           "The file is read when the map opens; changes made through the API (SetTerrainSettings) "
           "apply immediately. The cache/terrain folder does not need to be deleted.\n"
           "Delete this file to get the default colors back.";
}

// --- Merkezi ayarlar -------------------------------------------------------------------

const prime::LibrarySettings& MapData::LibrarySettings() const
{
    return mSettings;
}

void MapData::SetLibrarySettings(const prime::LibrarySettings& pSettings)
{
    mSettings = Sanitized(pSettings);
    ApplyLimits();
    if (!SaveLibrarySettings())
    {
        Log("WARNING: could not write {}", SettingsPath());
    }
}

std::string MapData::SettingsPath() const
{
    return mConfig.mDataDirectory + kSettingsFileName;
}

const std::string& MapData::DataDirectory() const
{
    return mConfig.mDataDirectory;
}

const std::string& MapData::CacheDirectory() const
{
    return mConfig.mCacheDirectory;
}

std::string MapData::MapsDirectory() const
{
    return mConfig.mDataDirectory + kMapsFolder;
}

void MapData::LoadLibrarySettings()
{
    // Dosyadaki deger her zaman onceliklidir; dosyada olmayan anahtar host'un verdigi ilk degeri alir.
    // Dosya yoksa (ilk acilis) olusturulur: kullanici acip duzenleyebilsin.
    Settings tFile;
    const bool tExists = tFile.Load(SettingsPath());
    prime::LibrarySettings tSettings = mSettings;
    tSettings.mTerrainCacheMb = tFile.GetInt(kCacheSection, "terrain_mb", tSettings.mTerrainCacheMb);
    tSettings.mCacheGrowthPercentPerMap =
        tFile.GetInt(kCacheSection, "growth_percent_per_map", tSettings.mCacheGrowthPercentPerMap);
    tSettings.mTerrainCacheMaxMb = tFile.GetInt(kCacheSection, "max_terrain_mb", tSettings.mTerrainCacheMaxMb);
    tSettings.mPrefetchTiles = tFile.GetInt(kCacheSection, "prefetch_tiles", tSettings.mPrefetchTiles);
    tSettings.mTextureMemoryMb = tFile.GetInt(kMemorySection, "texture_mb", tSettings.mTextureMemoryMb);
    tSettings.mMemoryGrowthPercentPerMap =
        tFile.GetInt(kMemorySection, "growth_percent_per_map", tSettings.mMemoryGrowthPercentPerMap);
    tSettings.mTextureMemoryMaxMb = tFile.GetInt(kMemorySection, "max_texture_mb", tSettings.mTextureMemoryMaxMb);
    tSettings.mAutoSave = tFile.GetBool(kMapsSection, "autosave", tSettings.mAutoSave);
    tSettings.mAutoSaveSeconds = tFile.GetDouble(kMapsSection, "autosave_seconds", tSettings.mAutoSaveSeconds);
    mSettings = Sanitized(tSettings);
    Log("central settings {}: {}", tExists ? "loaded" : "created", SettingsPath());
    if (!SaveLibrarySettings())  // eksik anahtarlar ve aciklama dosyada gorunsun
    {
        Log("WARNING: could not write {}", SettingsPath());
    }
}

bool MapData::SaveLibrarySettings() const
{
    Settings tFile;
    tFile.SetPath(SettingsPath());
    tFile.SetComment(kSettingsComment);
    tFile.SetInt(kCacheSection, "terrain_mb", mSettings.mTerrainCacheMb);
    tFile.SetInt(kCacheSection, "growth_percent_per_map", mSettings.mCacheGrowthPercentPerMap);
    tFile.SetInt(kCacheSection, "max_terrain_mb", mSettings.mTerrainCacheMaxMb);
    tFile.SetInt(kCacheSection, "prefetch_tiles", mSettings.mPrefetchTiles);
    tFile.SetInt(kMemorySection, "texture_mb", mSettings.mTextureMemoryMb);
    tFile.SetInt(kMemorySection, "growth_percent_per_map", mSettings.mMemoryGrowthPercentPerMap);
    tFile.SetInt(kMemorySection, "max_texture_mb", mSettings.mTextureMemoryMaxMb);
    tFile.SetBool(kMapsSection, "autosave", mSettings.mAutoSave);
    tFile.SetDouble(kMapsSection, "autosave_seconds", mSettings.mAutoSaveSeconds);
    return tFile.Save();
}

void MapData::ApplyLimits()
{
    // Acik harita sayisina gore (en az 1): disk siniri, doku butcesi, iscinin bellek listeleri
    const int tOpenMaps = std::max(1, OpenMapCount());
    const double tDiskMb = GrownLimit(mSettings.mTerrainCacheMb, mSettings.mCacheGrowthPercentPerMap,
                                      mSettings.mTerrainCacheMaxMb, tOpenMaps);
    const double tTextureMb = GrownLimit(mSettings.mTextureMemoryMb, mSettings.mMemoryGrowthPercentPerMap,
                                         mSettings.mTextureMemoryMaxMb, tOpenMaps);
    const double tMemoryScale = 1.0 + mSettings.mMemoryGrowthPercentPerMap / 100.0 * (tOpenMaps - 1);
    mTerrainStore.SetCacheLimit(static_cast<uint64_t>(tDiskMb * 1048576.0));
    mTerrainStore.SetMemoryScale(tMemoryScale);
    mHub.SetTextureBudget(static_cast<size_t>(tTextureMb * 1048576.0));
    mAirportDetails->SetMaxLoaded(
        static_cast<size_t>(std::lround(static_cast<double>(AirportDetailStore::kMaxLoaded) * tMemoryScale)));

    const int tDiskLimit = static_cast<int>(std::lround(tDiskMb));
    const int tTextureLimit = static_cast<int>(std::lround(tTextureMb));
    if (tDiskLimit != mStatus.mTerrainCacheLimitMb || tTextureLimit != mStatus.mTextureLimitMb ||
        OpenMapCount() != mStatus.mOpenMaps)
    {
        Log("cache limits for {} open map(s): terrain disk {} MB{}, terrain textures {} MB, memory scale {:.2f}",
            OpenMapCount(), tDiskLimit, tDiskLimit == 0 ? " (no limit)" : "", tTextureLimit, tMemoryScale);
    }
    mStatus.mTerrainCacheLimitMb = tDiskLimit;
    mStatus.mTextureLimitMb = tTextureLimit;
    mStatus.mOpenMaps = OpenMapCount();
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
    mHub.BeginFrame();  // bu karenin doku yukleme hakki (butun haritalar icin ortak)
    UpdatePrefetch(tNow);
    UpdateStatus();
    // Haritalar: ucak, katman verisi, durum yazisi, ayar dosyalari. Kopya liste: bir harita
    // guncellenirken (host'un callback'i yok ama ileride olabilir) liste degisirse bozulmasin.
    const std::vector<MapInstance*> tInstances = mInstances;
    bool tDiagnostics = false;
    for (MapInstance* tInstance : tInstances)
    {
        tInstance->Update(tNow);
        tDiagnostics = tDiagnostics || tInstance->IsDiagnosticsOn();
    }
    mTerrainStore.SetDiagnostics(tDiagnostics);  // asama sureleri sadece teshis modunda loglanir
    // Toplam bekleyen zemin (API Status): haritalar bu karede guncelledi
    int tPending = 0;
    for (const MapInstance* tInstance : tInstances)
    {
        tPending += tInstance->PendingTerrainImages();
    }
    mStatus.mTerrainPendingImages = tPending;
    mStatus.mText = mSharedText;
    if (tPending > 0 && !mTerrainIdle)
    {
        AddPart(&mStatus.mText, std::format("Loading terrain: {} image{}", tPending, tPending == 1 ? "" : "s"));
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
        mTerrainStore.Start(mXPlaneDirectory, mConfig.mCacheDirectory + kTerrainCacheFolder, mDefaultPalette,
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
    mStatus.mNavReady = mNavLoader.IsDone();
    mStatus.mAirportsReady = !mAirportGeometry->IsLoading() && mAirportGeometry->Count() > 0;
    mStatus.mAirwaysReady = mAirways.IsReady();
    mStatus.mTerrainIndexReady = mTerrainStore.IsIndexReady();

    // Ortak kisim: her haritanin alt seridinde ayni
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
    // Bolge onbellegi ilerlemesi ve bitisi
    const bool tPrecaching = mTerrainStore.PrecacheProgress(&mStatus.mPrecacheDone, &mStatus.mPrecacheTotal);
    if (tPrecaching)
    {
        AddPart(&tMessage, std::format("Pre-caching terrain: {}/{}", mStatus.mPrecacheDone, mStatus.mPrecacheTotal));
    }
    else if (mPrecacheRunning)
    {
        Log("terrain pre-cache finished: {} tiles", mStatus.mPrecacheDone);
        ShowNotice(std::format("Terrain pre-cache finished: {} tiles", mStatus.mPrecacheDone));
    }
    mPrecacheRunning = tPrecaching;
    mStatus.mPrecacheRunning = tPrecaching;
    if (!mNotice.empty() && XPLMGetElapsedTime() < mNoticeUntil)
    {
        AddPart(&tMessage, mNotice);
    }
    mSharedText = tMessage;
    // Zemin iscisinin o anki isi ("Building terrain +47-123"): haritalar kendi bekleyen sayisiyla yazar
    mTerrainWorkerText = mTerrainStore.StatusText();
    mTerrainIdle = mTerrainStore.IsIdle();
    // Harita resmi onbellegi: veri gelirken resim hiz sinirinda yenilensin (MapImageCache)
    mBusy = !mNavLoader.IsDone() || mAirportGeometry->IsLoading() || mAirportDetails->PendingCount() > 0 ||
            mAirways.IsLoading() || !mTerrainIdle;
}

bool MapData::IsBusy() const
{
    return mBusy;
}

const std::string& MapData::SharedStatusText() const
{
    return mSharedText;
}

const std::string& MapData::TerrainWorkerStatus() const
{
    return mTerrainWorkerText;
}

bool MapData::IsTerrainIdle() const
{
    return mTerrainIdle;
}

void MapData::ShowNotice(const std::string& pText)
{
    mNotice = pText;
    mNoticeUntil = XPLMGetElapsedTime() + kNoticeSeconds;
}

void MapData::UpdatePrefetch(float pNow)
{
    if (pNow - mLastPrefetch < kPrefetchPeriod || mSettings.mPrefetchTiles <= 0 || !mTerrainStore.IsIndexReady())
    {
        return;
    }
    mLastPrefetch = pNow;
    std::vector<int> tTiles;
    auto tAdd = [this, &tTiles](const LatLon& pPoint)
    {
        const int tWest = static_cast<int>(std::floor(pPoint.mLon));
        const int tSouth = static_cast<int>(std::floor(pPoint.mLat));
        if (static_cast<int>(tTiles.size()) >= mSettings.mPrefetchTiles || tSouth < -90 || tSouth > 89 ||
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
    // Her acik haritanin ucagi ve rotasi (cogu zaman ayni ucak: tekrarlar tAdd'de elenir)
    for (MapInstance* tInstance : mInstances)
    {
        if (!tInstance->IsOpen())
        {
            continue;
        }
        // Ucagin onu: hareket ediyorsa gidis yonunde 120 NM'ye kadar
        const AircraftState& tAircraft = tInstance->Aircraft().State();
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
        const std::vector<LatLon> tRoute = tInstance->Fms() ? tInstance->Fms()->RouteAhead() : std::vector<LatLon>();
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
    const int tLimitMb = mStatus.mTerrainCacheLimitMb;  // acik harita sayisina gore buyumus sinir
    if (tLimitMb > 0 && tEstimateMb > tLimitMb * kPrecacheLimitShare)
    {
        ShowNotice(std::format("Pre-cache: area too large ({} tiles, ~{:.0f} MB; cache limit {} MB). Zoom in "
                               "or raise [cache] terrain_mb in prime_map.ini",
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
