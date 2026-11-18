#include "engine/XMapData.h"

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
#include "XMapTheme.h"
#include "data/AptDatIndexer.h"
#include "data/SceneryIndex.h"
#include "data/TerrainPaletteSource.h"
#include "api/XMap.h"
#include "layers/FmsLayer.h"

namespace
{
const char* kTerrainCacheFolder = "terrain/";
const char* kAirportCacheFolder = "airports/";
const char* kSettingsFileName = "prime_map.json";
const char* kMapsFolder = "maps/";
const char* kCacheSection = "cache";
const char* kMemorySection = "memory";
const char* kMapsSection = "maps";
const float kPrefetchPeriod = 5.0f;
const double kTileBytesEstimate = 3.5 * 1048576.0;
const double kPrecacheLimitShare = 0.8;
const float kNoticeSeconds = 8.0f;
const double kNavLoadBudgetMs = 4.0;

std::string systemPath()
{
    std::array<char, 2048> tSystemPath = {};
    XPLMGetSystemPath(tSystemPath.data());
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
        tLimit = std::min(tLimit, std::max(pMax, pBase));
    }
    return tLimit;
}

prime::XMapManagerSettings sanitized(prime::XMapManagerSettings pSettings)
{
    pSettings.mTerrainCacheMb = std::max(0, pSettings.mTerrainCacheMb);
    pSettings.mCacheGrowthPercentPerXMap = std::clamp(pSettings.mCacheGrowthPercentPerXMap, 0, 1000);
    pSettings.mTerrainCacheMaxMb = std::max(0, pSettings.mTerrainCacheMaxMb);
    pSettings.mPrefetchTiles = std::clamp(pSettings.mPrefetchTiles, 0, 500);
    pSettings.mTextureMemoryMb = std::clamp(pSettings.mTextureMemoryMb, 64, 16384);
    pSettings.mMemoryGrowthPercentPerXMap = std::clamp(pSettings.mMemoryGrowthPercentPerXMap, 0, 1000);
    pSettings.mTextureMemoryMaxMb = std::max(0, pSettings.mTextureMemoryMaxMb);
    pSettings.mAutoSaveSeconds = std::clamp(pSettings.mAutoSaveSeconds, 0.0, 3600.0);
    return pSettings;
}
}

XMapData::XMapData(const XMapDataConfig& pConfig)
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
    mAircraft.init();

    std::error_code tError;
    std::filesystem::create_directories(mConfig.mDataDirectory, tError);
    loadLibrarySettings();
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
    mTerrainStore.stop();
    mAirportDetails->stop();
}

void XMapData::addInstance(prime::XMap* pInstance)
{
    if (pInstance && std::find(mInstances.begin(), mInstances.end(), pInstance) == mInstances.end())
    {
        mInstances.push_back(pInstance);
    }
}

void XMapData::removeInstance(prime::XMap* pInstance)
{
    mInstances.erase(std::remove(mInstances.begin(), mInstances.end(), pInstance), mInstances.end());
    applyLimits();
}

void XMapData::openXMapsChanged()
{
    applyLimits();
}

int XMapData::openXMapCount() const
{
    return static_cast<int>(std::count_if(mInstances.begin(), mInstances.end(),
                                          [](const prime::XMap* pInstance)
                                          {
                                              return pInstance->isInitialized();
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

const XMapDataStatus& XMapData::status() const
{
    return mStatus;
}

int XMapData::registerPalette(TerrainPalette* pPalette)
{
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

const prime::XMapManagerSettings& XMapData::managerSettings() const
{
    return mSettings;
}

void XMapData::setManagerSettings(const prime::XMapManagerSettings& pSettings)
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
    Settings tFile;
    const bool tExists = tFile.load(settingsPath());
    prime::XMapManagerSettings tSettings = mSettings;
    tSettings.mTerrainCacheMb = tFile.getInt(kCacheSection, "terrain_mb", tSettings.mTerrainCacheMb);
    tSettings.mCacheGrowthPercentPerXMap =
        tFile.getInt(kCacheSection, "growth_percent_per_map", tSettings.mCacheGrowthPercentPerXMap);
    tSettings.mTerrainCacheMaxMb = tFile.getInt(kCacheSection, "max_terrain_mb", tSettings.mTerrainCacheMaxMb);
    tSettings.mPrefetchTiles = tFile.getInt(kCacheSection, "prefetch_tiles", tSettings.mPrefetchTiles);
    tSettings.mTextureMemoryMb = tFile.getInt(kMemorySection, "texture_mb", tSettings.mTextureMemoryMb);
    tSettings.mMemoryGrowthPercentPerXMap =
        tFile.getInt(kMemorySection, "growth_percent_per_map", tSettings.mMemoryGrowthPercentPerXMap);
    tSettings.mTextureMemoryMaxMb = tFile.getInt(kMemorySection, "max_texture_mb", tSettings.mTextureMemoryMaxMb);
    tSettings.mAutoSave = tFile.getBool(kMapsSection, "autosave", tSettings.mAutoSave);
    tSettings.mAutoSaveSeconds = tFile.getDouble(kMapsSection, "autosave_seconds", tSettings.mAutoSaveSeconds);
    mSettings = sanitized(tSettings);
    log("central settings {}: {}", tExists ? "loaded" : "created", settingsPath());
    if (!saveLibrarySettings())
    {
        log("WARNING: could not write {}", settingsPath());
    }
}

bool XMapData::saveLibrarySettings() const
{
    Settings tFile;
    tFile.setPath(settingsPath());

    tFile.setInt(kCacheSection, "terrain_mb", mSettings.mTerrainCacheMb);
    tFile.setInt(kCacheSection, "growth_percent_per_map", mSettings.mCacheGrowthPercentPerXMap);
    tFile.setInt(kCacheSection, "max_terrain_mb", mSettings.mTerrainCacheMaxMb);
    tFile.setInt(kCacheSection, "prefetch_tiles", mSettings.mPrefetchTiles);
    tFile.setInt(kMemorySection, "texture_mb", mSettings.mTextureMemoryMb);
    tFile.setInt(kMemorySection, "growth_percent_per_map", mSettings.mMemoryGrowthPercentPerXMap);
    tFile.setInt(kMemorySection, "max_texture_mb", mSettings.mTextureMemoryMaxMb);
    tFile.setBool(kMapsSection, "autosave", mSettings.mAutoSave);
    tFile.setDouble(kMapsSection, "autosave_seconds", mSettings.mAutoSaveSeconds);
    return tFile.save();
}

void XMapData::applyLimits()
{
    const int tOpenMaps = std::max(1, openXMapCount());
    const double tDiskMb = grownLimit(mSettings.mTerrainCacheMb, mSettings.mCacheGrowthPercentPerXMap,
                                      mSettings.mTerrainCacheMaxMb, tOpenMaps);
    const double tTextureMb = grownLimit(mSettings.mTextureMemoryMb, mSettings.mMemoryGrowthPercentPerXMap,
                                         mSettings.mTextureMemoryMaxMb, tOpenMaps);
    const double tMemoryScale = 1.0 + mSettings.mMemoryGrowthPercentPerXMap / 100.0 * (tOpenMaps - 1);
    mTerrainStore.setCacheLimit(static_cast<uint64_t>(tDiskMb * 1048576.0));
    mTerrainStore.setMemoryScale(tMemoryScale);
    mHub.setTextureBudget(static_cast<size_t>(tTextureMb * 1048576.0));
    mAirportDetails->setMaxLoaded(
        static_cast<size_t>(std::lround(static_cast<double>(AirportDetailStore::kMaxLoaded) * tMemoryScale)));
    const int tDiskLimit = static_cast<int>(std::lround(tDiskMb));
    const int tTextureLimit = static_cast<int>(std::lround(tTextureMb));
    if (tDiskLimit != mStatus.mTerrainCacheLimitMb || tTextureLimit != mStatus.mTextureLimitMb ||
        openXMapCount() != mStatus.mOpenMaps)
    {
        log("cache limits for {} open map(s): terrain disk {} MB{}, terrain textures {} MB, memory scale {:.2f}",
            openXMapCount(), tDiskLimit, tDiskLimit == 0 ? " (no limit)" : "", tTextureLimit, tMemoryScale);
    }
    mStatus.mTerrainCacheLimitMb = tDiskLimit;
    mStatus.mTextureLimitMb = tTextureLimit;
    mStatus.mOpenMaps = openXMapCount();
}

float XMapData::flightLoopCallback(float , float , int ,
                                  void* pRefcon)
{
    return static_cast<XMapData*>(pRefcon)->onFlightLoop();
}

float XMapData::onFlightLoop()
{
    const float tNow = XPLMGetElapsedTime();
    stepLoading();
    mHub.beginFrame();
    const bool tAnyOpenMap =
        std::any_of(mInstances.begin(), mInstances.end(),
                    [](const prime::XMap* pInstance)
                    {
                        return pInstance->isInitialized();
                    });
    if (!tAnyOpenMap)
    {
        mAircraft.refresh();
    }
    updatePrefetch(tNow);
    updateStatus();
    const std::vector<prime::XMap*> tInstances = mInstances;
    bool tDiagnostics = false;
    for (prime::XMap* tInstance : tInstances)
    {
        tInstance->update(tNow);
        tDiagnostics = tDiagnostics || tInstance->isDiagnosticsOn();
    }
    mTerrainStore.setDiagnostics(tDiagnostics);
    int tPending = 0;
    for (const prime::XMap* tInstance : tInstances)
    {
        tPending += tInstance->getTerrainPendingImages();
    }
    mStatus.mTerrainPendingImages = tPending;
    mStatus.mText = mSharedText;
    if (tPending > 0 && !mTerrainIdle)
    {
        addPart(&mStatus.mText, std::format("Loading terrain: {} image{}", tPending, tPending == 1 ? "" : "s"));
    }
    return -1.0f;
}

void XMapData::stepLoading()
{
    if (!mLoadStarted)
    {
        mLoadStarted = true;
        log("indexing airports in the background: {}", mAirportGeometry->sourceName());
        mAirportGeometry->startLoading(mXPlaneDirectory, mConfig.mCacheDirectory + kAirportCacheFolder);
        mTerrainStore.start(mXPlaneDirectory, mConfig.mCacheDirectory + kTerrainCacheFolder, mDefaultPalette,
                            allTerrainStyles());
    }
    logReport(mTerrainStore.takeLogLines());
    mAirportDetails->update();
    logReport(mAirportDetails->takeLogLines());
    if (mAirportGeometry->poll())
    {
        log("airport index {}: {} airports, {} runways", mAirportGeometry->lastLoadSucceeded() ? "ready" : "FAILED",
            mAirportGeometry->count(), mAirportGeometry->runwayCount());
        logReport(mAirportGeometry->lastReport());
        mAirportDetails->start();
    }
    if (!mNavLoader.isDone())
    {
        if (mNavLoader.step(kNavLoadBudgetMs))
        {
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
    mTerrainWorkerText = mTerrainStore.statusText();
    mTerrainIdle = mTerrainStore.isIdle();
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
    for (prime::XMap* tInstance : mInstances)
    {
        if (!tInstance->isInitialized())
        {
            continue;
        }
        const AircraftState& tAircraft = tInstance->mAircraft->state();
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
        const std::vector<LatLon> tRoute =
            tInstance->mFmsLayer ? tInstance->mFmsLayer->routeAhead() : std::vector<LatLon>();
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
    if (tTiles.empty())
    {
        const AircraftState& tAircraft = mAircraft.state();
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
            if (!tTiles.empty() && !mMaplessPrefetchLogged)
            {
                mMaplessPrefetchLogged = true;
                log("prefetch without open maps: {} tile(s) around the aircraft (setPrefetchTiles(0) disables)",
                    tTiles.size());
            }
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
    const double tEstimateMb = static_cast<double>(tTiles.size()) * kTileBytesEstimate / 1048576.0;
    const int tLimitMb = mStatus.mTerrainCacheLimitMb;
    if (tLimitMb > 0 && tEstimateMb > tLimitMb * kPrecacheLimitShare)
    {
        showNotice(std::format("Pre-cache: area too large ({} tiles, ~{:.0f} MB; cache limit {} MB). Zoom in "
                               "or raise [cache] terrain_mb in prime_map.json",
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
