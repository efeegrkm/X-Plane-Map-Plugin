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
    "Prime XMap central settings (shared by ALL maps of this X-Plane process).\n"
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
    "the map is closed or the host calls XMap::saveSettings.\n"
    "[maps] autosave_seconds = shortest time between two automatic writes of the same map.\n"
    "Edit while X-Plane is closed, or change at run time with XMapManager::setSettings.";

std::string systemPath()
{
    std::array<char, 2048> tSystemPath = {};
    XPLMGetSystemPath(tSystemPath.data());  // "C:\X-Plane 11\" (sonunda ayirici ile)
    return tSystemPath.data();
}

void addPart(std::string* pMessage, const std::string& pPart)
{
    if (pPart.empty())
    {
        return;
    }
    *pMessage += pMessage->empty() ? "" : "   ";
    *pMessage += pPart;
}

// taban x (1 + artis% x (n - 1) / 100); tavan > 0 ise onu gecmez. taban 0 = sinirsiz (0 doner).
double grownLimit(double pBase, int pGrowthPercent, double pMax, int pOpenMaps)
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

prime::MapManagerSettings sanitized(prime::MapManagerSettings pSettings)
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

XMapData::XMapData(const MapDataConfig& pConfig)
    : mConfig(pConfig)
    , mSettings(sanitized(pConfig.mDefaults))
    , mAirportGeometry(std::make_unique<AirportGeometryStore>(std::make_unique<AptDatIndexer>()))
{
    mAirportDetails = std::make_unique<AirportDetailStore>(*mAirportGeometry);
}

XMapData::~XMapData()
{
    stop();
}

void XMapData::start()
{
    mXPlaneDirectory = systemPath();
    std::error_code tError;
    std::filesystem::create_directories(mConfig.mDataDirectory, tError);
    loadLibrarySettings();
    // Palet 0: varsayilan zemin ayarlari (terrain_colors.ini'si olmayan haritalar bunu kullanir)
    registerPalette(&mDefaultPalette);
    applyLimits();
    XPLMRegisterFlightLoopCallback(flightLoopCallback, -1.0f, this);
    mFlightLoopRegistered = true;
    mRunning = true;
}

void XMapData::stop()
{
    mRunning = false;
    if (mFlightLoopRegistered)
    {
        XPLMUnregisterFlightLoopCallback(flightLoopCallback, this);
        mFlightLoopRegistered = false;
    }
    mTerrainStore.stop();  // zemin iscisi: o an uretilen karoyu bitirip durur
    mAirportDetails->stop();
}

void XMapData::addInstance(XMapInstance* pInstance)
{
    if (pInstance && std::find(mInstances.begin(), mInstances.end(), pInstance) == mInstances.end())
    {
        mInstances.push_back(pInstance);
    }
}

void XMapData::removeInstance(XMapInstance* pInstance)
{
    mInstances.erase(std::remove(mInstances.begin(), mInstances.end(), pInstance), mInstances.end());
    applyLimits();
}

void XMapData::openMapsChanged()
{
    applyLimits();
}

int XMapData::openMapCount() const
{
    return static_cast<int>(std::count_if(mInstances.begin(), mInstances.end(),
                                          [](const XMapInstance* pInstance)
                                          {
                                              return pInstance->isOpen();
                                          }));
}

TerrainHub& XMapData::hub()
{
    return mHub;
}

TerrainStore& XMapData::terrain()
{
    return mTerrainStore;
}

NavDatabase& XMapData::navs()
{
    return mNavDatabase;
}

AirwayDatabase& XMapData::airways()
{
    return mAirways;
}

AirportGeometryStore& XMapData::airportGeometry()
{
    return *mAirportGeometry;
}

AirportDetailStore& XMapData::airportDetails()
{
    return *mAirportDetails;
}

const AirportGeometryStore& XMapData::airports() const
{
    return *mAirportGeometry;
}

const MapDataStatus& XMapData::status() const
{
    return mStatus;
}

int XMapData::registerPalette(TerrainPalette* pPalette)
{
    // Sinif renkleri X-Plane'in arazi dokularindan turetilir: ilk seferde hesaplanir,
    // sonra onbellekten okunur; sonuc butun haritalar icin ayni
    if (pPalette->usesXPlaneColors())
    {
        if (!mDerivedLoaded)
        {
            mDerivedLoaded = true;
            XPlaneClassColors tDerived;
            std::string tReport;
            mDerivedOk = loadXPlaneClassColors(systemPath(),
                                               mConfig.mCacheDirectory + kTerrainCacheFolder + "xplane_palette.txt",
                                               &tDerived, &tReport);
            mDerivedColors = tDerived.mColors;
            mDerivedCounts = tDerived.mTextureCounts;
            log("{}", tReport);
        }
        if (mDerivedOk)
        {
            pPalette->applyDerivedColors(mDerivedColors, mDerivedCounts);
        }
    }
    // Ayni icerik = ayni numara: ayni ayarli haritalar ayni dokulari paylasir
    Settings tText;
    pPalette->save(tText);
    const std::string tKey = tText.toText({});
    const auto tFound = mPaletteIds.find(tKey);
    if (tFound != mPaletteIds.end())
    {
        return tFound->second;
    }
    const int tId = mTerrainStore.addPalette(*pPalette);
    mPaletteIds.emplace(tKey, tId);
    return tId;
}

const char* XMapData::terrainSettingsComment()
{
    return "Prime XMap terrain colors and terrain settings of THIS map (each map has its own file).\n"
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
           "The file is read when the map opens; changes made through the API (setTerrainSettings) "
           "apply immediately. The cache/terrain folder does not need to be deleted.\n"
           "Delete this file to get the default colors back.";
}

const prime::MapManagerSettings& XMapData::managerSettings() const
{
    return mSettings;
}

void XMapData::setManagerSettings(const prime::MapManagerSettings& pSettings)
{
    mSettings = sanitized(pSettings);
    applyLimits();
    if (!saveLibrarySettings())
    {
        log("WARNING: could not write {}", settingsPath());
    }
}

std::string XMapData::settingsPath() const
{
    return mConfig.mDataDirectory + kSettingsFileName;
}

const std::string& XMapData::dataDirectory() const
{
    return mConfig.mDataDirectory;
}

const std::string& XMapData::cacheDirectory() const
{
    return mConfig.mCacheDirectory;
}

std::string XMapData::mapsDirectory() const
{
    return mConfig.mDataDirectory + kMapsFolder;
}

void XMapData::loadLibrarySettings()
{
    // Dosyadaki deger her zaman onceliklidir; dosyada olmayan anahtar host'un verdigi ilk degeri alir.
    // Dosya yoksa (ilk acilis) olusturulur: kullanici acip duzenleyebilsin.
    Settings tFile;
    const bool tExists = tFile.load(settingsPath());
    prime::MapManagerSettings tSettings = mSettings;
    tSettings.mTerrainCacheMb = tFile.getInt(kCacheSection, "terrain_mb", tSettings.mTerrainCacheMb);
    tSettings.mCacheGrowthPercentPerMap =
        tFile.getInt(kCacheSection, "growth_percent_per_map", tSettings.mCacheGrowthPercentPerMap);
    tSettings.mTerrainCacheMaxMb = tFile.getInt(kCacheSection, "max_terrain_mb", tSettings.mTerrainCacheMaxMb);
    tSettings.mPrefetchTiles = tFile.getInt(kCacheSection, "prefetch_tiles", tSettings.mPrefetchTiles);
    tSettings.mTextureMemoryMb = tFile.getInt(kMemorySection, "texture_mb", tSettings.mTextureMemoryMb);
    tSettings.mMemoryGrowthPercentPerMap =
        tFile.getInt(kMemorySection, "growth_percent_per_map", tSettings.mMemoryGrowthPercentPerMap);
    tSettings.mTextureMemoryMaxMb = tFile.getInt(kMemorySection, "max_texture_mb", tSettings.mTextureMemoryMaxMb);
    tSettings.mAutoSave = tFile.getBool(kMapsSection, "autosave", tSettings.mAutoSave);
    tSettings.mAutoSaveSeconds = tFile.getDouble(kMapsSection, "autosave_seconds", tSettings.mAutoSaveSeconds);
    mSettings = sanitized(tSettings);
    log("central settings {}: {}", tExists ? "loaded" : "created", settingsPath());
    if (!saveLibrarySettings())  // eksik anahtarlar ve aciklama dosyada gorunsun
    {
        log("WARNING: could not write {}", settingsPath());
    }
}

bool XMapData::saveLibrarySettings() const
{
    Settings tFile;
    tFile.setPath(settingsPath());
    tFile.setComment(kSettingsComment);
    tFile.setInt(kCacheSection, "terrain_mb", mSettings.mTerrainCacheMb);
    tFile.setInt(kCacheSection, "growth_percent_per_map", mSettings.mCacheGrowthPercentPerMap);
    tFile.setInt(kCacheSection, "max_terrain_mb", mSettings.mTerrainCacheMaxMb);
    tFile.setInt(kCacheSection, "prefetch_tiles", mSettings.mPrefetchTiles);
    tFile.setInt(kMemorySection, "texture_mb", mSettings.mTextureMemoryMb);
    tFile.setInt(kMemorySection, "growth_percent_per_map", mSettings.mMemoryGrowthPercentPerMap);
    tFile.setInt(kMemorySection, "max_texture_mb", mSettings.mTextureMemoryMaxMb);
    tFile.setBool(kMapsSection, "autosave", mSettings.mAutoSave);
    tFile.setDouble(kMapsSection, "autosave_seconds", mSettings.mAutoSaveSeconds);
    return tFile.save();
}

void XMapData::applyLimits()
{
    const int tOpenMaps = std::max(1, openMapCount());
    const double tDiskMb = grownLimit(mSettings.mTerrainCacheMb, mSettings.mCacheGrowthPercentPerMap,
                                      mSettings.mTerrainCacheMaxMb, tOpenMaps);
    const double tTextureMb = grownLimit(mSettings.mTextureMemoryMb, mSettings.mMemoryGrowthPercentPerMap,
                                         mSettings.mTextureMemoryMaxMb, tOpenMaps);
    const double tMemoryScale = 1.0 + mSettings.mMemoryGrowthPercentPerMap / 100.0 * (tOpenMaps - 1);
    mTerrainStore.setCacheLimit(static_cast<uint64_t>(tDiskMb * 1048576.0));
    mTerrainStore.setMemoryScale(tMemoryScale);
    mHub.setTextureBudget(static_cast<size_t>(tTextureMb * 1048576.0));
    mAirportDetails->setMaxLoaded(
        static_cast<size_t>(std::lround(static_cast<double>(AirportDetailStore::kMaxLoaded) * tMemoryScale)));

    const int tDiskLimit = static_cast<int>(std::lround(tDiskMb));
    const int tTextureLimit = static_cast<int>(std::lround(tTextureMb));
    if (tDiskLimit != mStatus.mTerrainCacheLimitMb || tTextureLimit != mStatus.mTextureLimitMb ||
        openMapCount() != mStatus.mOpenMaps)
    {
        log("cache limits for {} open map(s): terrain disk {} MB{}, terrain textures {} MB, memory scale {:.2f}",
            openMapCount(), tDiskLimit, tDiskLimit == 0 ? " (no limit)" : "", tTextureLimit, tMemoryScale);
    }
    mStatus.mTerrainCacheLimitMb = tDiskLimit;
    mStatus.mTextureLimitMb = tTextureLimit;
    mStatus.mOpenMaps = openMapCount();
}

float XMapData::flightLoopCallback(float /*pSinceLastCall*/, float /*pSinceLastLoop*/, int /*pCounter*/,
                                  void* pRefcon)
{
    return static_cast<XMapData*>(pRefcon)->onFlightLoop();
}

float XMapData::onFlightLoop()
{
    const float tNow = XPLMGetElapsedTime();
    stepLoading();
    mHub.beginFrame();  // bu karenin doku yukleme hakki (butun haritalar icin ortak)
    updatePrefetch(tNow);
    updateStatus();
    // Haritalar: ucak, katman verisi, durum yazisi, ayar dosyalari. Kopya liste: bir harita
    // guncellenirken (host'un callback'i yok ama ileride olabilir) liste degisirse bozulmasin.
    const std::vector<XMapInstance*> tInstances = mInstances;
    bool tDiagnostics = false;
    for (XMapInstance* tInstance : tInstances)
    {
        tInstance->update(tNow);
        tDiagnostics = tDiagnostics || tInstance->isDiagnosticsOn();
    }
    mTerrainStore.setDiagnostics(tDiagnostics);  // asama sureleri sadece teshis modunda loglanir
    int tPending = 0;
    for (const XMapInstance* tInstance : tInstances)
    {
        tPending += tInstance->pendingTerrainImages();
    }
    mStatus.mTerrainPendingImages = tPending;
    mStatus.mText = mSharedText;
    if (tPending > 0 && !mTerrainIdle)
    {
        addPart(&mStatus.mText, std::format("Loading terrain: {} image{}", tPending, tPending == 1 ? "" : "s"));
    }
    return -1.0f;  // her karede tekrar cagir
}

void XMapData::stepLoading()
{
    // Ilk flight loop'ta (X-Plane hazir) arka plan islerini baslat
    if (!mLoadStarted)
    {
        mLoadStarted = true;
        log("indexing airports in the background: {}", mAirportGeometry->sourceName());
        mAirportGeometry->startLoading(mXPlaneDirectory, mConfig.mCacheDirectory + kAirportCacheFolder);
        mTerrainStore.start(mXPlaneDirectory, mConfig.mCacheDirectory + kTerrainCacheFolder, mDefaultPalette,
                            allTerrainStyles());
    }
    logReport(mTerrainStore.takeLogLines());
    mAirportDetails->update();  // onceki karede istenen havalimani ayrintilari isciye
    logReport(mAirportDetails->takeLogLines());
    if (mAirportGeometry->poll())
    {
        log("airport index {}: {} airports, {} runways", mAirportGeometry->lastLoadSucceeded() ? "ready" : "FAILED",
            mAirportGeometry->count(), mAirportGeometry->runwayCount());
        logReport(mAirportGeometry->lastReport());
        mAirportDetails->start();  // ayrintilar artik dosyadaki yerlerinden okunabilir
    }

    // Nav verisi: ana thread'de, her karede birkac ms
    if (!mNavLoader.isDone())
    {
        if (mNavLoader.step(kNavLoadBudgetMs))
        {
            // Hava yollarinin uc noktalari nav verisinden bulunur: nav bitince basla.
            // X-Plane'in kurali: once Custom data(guncel AIRAC), yoksa varsayilan.
            mAirways.startLoading({mXPlaneDirectory + "Custom Data/earth_awy.dat",
                                   mXPlaneDirectory + "Resources/default data/earth_awy.dat"},
                                  mNavDatabase);
        }
    }
    if (mAirways.poll())
    {
        log("airways {}: {}", mAirways.isReady() ? "ready" : "FAILED", mAirways.lastReport());
    }
}

void XMapData::updateStatus()
{
    mStatus.mNavReady = mNavLoader.isDone();
    mStatus.mAirportsReady = !mAirportGeometry->isLoading() && mAirportGeometry->count() > 0;
    mStatus.mAirwaysReady = mAirways.isReady();
    mStatus.mTerrainIndexReady = mTerrainStore.isIndexReady();

    // Ortak kisim: her haritanin alt seridinde ayni
    std::string tMessage;
    if (!mNavLoader.isDone() && mNavLoader.visitedCount() > 0)
    {
        addPart(&tMessage, std::format("Loading nav data: {}", mNavLoader.visitedCount()));
    }
    if (mAirportGeometry->isLoading())
    {
        addPart(&tMessage, "Indexing airports...");
    }
    if (mAirportDetails->pendingCount() > 0)
    {
        addPart(&tMessage, std::format("Loading airport details: {}", mAirportDetails->pendingCount()));
    }
    if (mAirways.isLoading())
    {
        addPart(&tMessage, "Reading airways...");
    }
    const bool tPrecaching = mTerrainStore.precacheProgress(&mStatus.mPrecacheDone, &mStatus.mPrecacheTotal);
    if (tPrecaching)
    {
        addPart(&tMessage, std::format("Pre-caching terrain: {}/{}", mStatus.mPrecacheDone, mStatus.mPrecacheTotal));
    }
    else if (mPrecacheRunning)
    {
        log("terrain pre-cache finished: {} tiles", mStatus.mPrecacheDone);
        showNotice(std::format("Terrain pre-cache finished: {} tiles", mStatus.mPrecacheDone));
    }
    mPrecacheRunning = tPrecaching;
    mStatus.mPrecacheRunning = tPrecaching;
    if (!mNotice.empty() && XPLMGetElapsedTime() < mNoticeUntil)
    {
        addPart(&tMessage, mNotice);
    }
    mSharedText = tMessage;
    // Zemin iscisinin o anki isi ("Building terrain +47-123"): haritalar kendi bekleyen sayisiyla yazar
    mTerrainWorkerText = mTerrainStore.statusText();
    mTerrainIdle = mTerrainStore.isIdle();
    // Harita resmi onbellegi: veri gelirken resim hiz sinirinda yenilensin (XMapImageCache)
    mBusy = !mNavLoader.isDone() || mAirportGeometry->isLoading() || mAirportDetails->pendingCount() > 0 ||
            mAirways.isLoading() || !mTerrainIdle;
}

bool XMapData::isBusy() const
{
    return mBusy;
}

const std::string& XMapData::sharedStatusText() const
{
    return mSharedText;
}

const std::string& XMapData::terrainWorkerStatus() const
{
    return mTerrainWorkerText;
}

bool XMapData::isTerrainIdle() const
{
    return mTerrainIdle;
}

void XMapData::showNotice(const std::string& pText)
{
    mNotice = pText;
    mNoticeUntil = XPLMGetElapsedTime() + kNoticeSeconds;
}

void XMapData::updatePrefetch(float pNow)
{
    if (pNow - mLastPrefetch < kPrefetchPeriod || mSettings.mPrefetchTiles <= 0 || !mTerrainStore.isIndexReady())
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
            mTerrainStore.coverageOf(tWest, tSouth) != TileCoverage::hasDsf)
        {
            return;
        }
        const int tKey = SceneryIndex::tileKey(tWest, tSouth);
        if (std::find(tTiles.begin(), tTiles.end(), tKey) == tTiles.end())
        {
            tTiles.push_back(tKey);
        }
    };
    // Her acik haritanin ucagi ve rotasi (cogu zaman ayni ucak: tekrarlar tAdd'de elenir)
    for (XMapInstance* tInstance : mInstances)
    {
        if (!tInstance->isOpen())
        {
            continue;
        }
        // Ucagin onu: hareket ediyorsa gidis yonunde 120 NM'ye kadar
        const AircraftState& tAircraft = tInstance->aircraft().state();
        LatLon tFrom = tAircraft.mPosition;
        if (tAircraft.mValid)
        {
            tAdd(tAircraft.mPosition);
            if (tAircraft.mGroundSpeedKt > 40.0)
            {
                for (double tDistance = 15.0; tDistance <= 120.0; tDistance += 15.0)
                {
                    tAdd(destinationPoint(tAircraft.mPosition, tAircraft.mHeadingDeg, tDistance));
                }
            }
        }
        // FMS rotasinin kalan kismi: bacaklar ~10 NM aralikla orneklenir
        const std::vector<LatLon> tRoute = tInstance->fms() ? tInstance->fms()->routeAhead() : std::vector<LatLon>();
        for (size_t tIndex = 0; tIndex < tRoute.size(); ++tIndex)
        {
            if (tIndex == 0 && !tAircraft.mValid)
            {
                tFrom = tRoute[0];
            }
            const double tLength = distanceNm(tFrom, tRoute[tIndex]);
            const double tBearing = initialBearingDeg(tFrom, tRoute[tIndex]);
            for (double tDistance = 0.0; tDistance < tLength; tDistance += 10.0)
            {
                tAdd(destinationPoint(tFrom, tBearing, tDistance));
            }
            tAdd(tRoute[tIndex]);
            tFrom = tRoute[tIndex];
        }
    }
    mTerrainStore.setPrefetchTiles(tTiles);
}

bool XMapData::startPrecache(const GeoBounds& pBounds)
{
    if (!mTerrainStore.isIndexReady())
    {
        showNotice("Pre-cache: scenery index is not ready yet");
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
            if (mTerrainStore.coverageOf(tLon, tLat) == TileCoverage::hasDsf)
            {
                tTiles.push_back(SceneryIndex::tileKey(tLon, tLat));
            }
        }
    }
    if (tTiles.empty())
    {
        showNotice("Pre-cache: no installed scenery in the visible area");
        return false;
    }
    // Onbellek siniriyla catismasin: sigmayacaksa hic baslama (CacheTrim yeni karolari silerdi)
    const double tEstimateMb = static_cast<double>(tTiles.size()) * kTileBytesEstimate / 1048576.0;
    const int tLimitMb = mStatus.mTerrainCacheLimitMb;  // acik harita sayisina gore buyumus sinir
    if (tLimitMb > 0 && tEstimateMb > tLimitMb * kPrecacheLimitShare)
    {
        showNotice(std::format("Pre-cache: area too large ({} tiles, ~{:.0f} MB; cache limit {} MB). Zoom in "
                               "or raise [cache] terrain_mb in prime_map.ini",
                               tTiles.size(), tEstimateMb, tLimitMb));
        return false;
    }
    mTerrainStore.startPrecache(tTiles);
    mPrecacheRunning = true;
    log("terrain pre-cache started: {} tiles (~{:.0f} MB)", tTiles.size(), tEstimateMb);
    return true;
}

void XMapData::cancelPrecache()
{
    if (mPrecacheRunning)
    {
        mTerrainStore.cancelPrecache();
        showNotice("Terrain pre-cache cancelled");
    }
}

void XMapData::logReport(const std::string& pReport)
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
            log("  {}", pReport.substr(tStart, tEnd - tStart));
        }
        tStart = tEnd + 1;
    }
}
