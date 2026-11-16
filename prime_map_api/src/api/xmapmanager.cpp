// xmapmanager.cpp: prime::XMapManager + XMapManagerFactory. Haritalarin sahibi.
// Veri klasoru bos birakilirsa %LOCALAPPDATA%/prime/prime-xmap kullanilir.
#include "PrimeMap.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <format>

#include "XPLMUtilities.h"

#include "Log.h"
#include "engine/LayerTable.h"
#include "engine/MapInstance.h"
#include "XMapApiUtil.h"
#include "XMapManagerPrivate.h"

namespace prime
{

std::string versionString()
{
    return std::format("{}.{}.{}", kVersionMajor, kVersionMinor, kVersionPatch);
}

XMapManager::XMapManager()
    : mPrivate(std::make_unique<XMapManagerPrivate>())
{
}

XMapManager::~XMapManager() = default;

void XMapManager::setDataDirectory(const std::string& pPath)
{
    mPrivate->mDataDirectory = pPath;
}

void XMapManager::setCacheDirectory(const std::string& pPath)
{
    mPrivate->mCacheDirectory = pPath;
}

void XMapManager::setLogPrefix(const std::string& pPrefix)
{
    mPrivate->mLogPrefix = pPrefix;
    ::setLogPrefix(pPrefix);
}

void XMapManager::setLogCallback(std::function<void(std::string_view pLine)> pCallback)
{
    mPrivate->mLogCallback = pCallback;
    ::setLogCallback(std::move(pCallback));
}

void XMapManager::setTerrainCacheMb(int pMb)
{
    mPrivate->mTerrainCacheMb = std::max(0, pMb);
}

void XMapManager::setPrefetchTiles(int pTiles)
{
    mPrivate->mPrefetchTiles = std::max(0, pTiles);
}

bool XMapManager::initialize()
{
    if (mPrivate->mData)
    {
        return true;  // zaten calisiyor; ayni yoneticiyi iki kez baslatmak anlamsiz
    }
    setLogPrefix(mPrivate->mLogPrefix);
    setLogCallback(mPrivate->mLogCallback);
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);

    MapDataConfig tConfig;
    std::string tDataDir = mPrivate->mDataDirectory;
    if (tDataDir.empty())
    {
        const char* tAppData = std::getenv("LOCALAPPDATA");
        tDataDir = (tAppData ? std::string(tAppData) : std::string(".")) + "/prime/prime-xmap";
    }
    tConfig.mDataDirectory = api::withSeparator(tDataDir);
    tConfig.mCacheDirectory = api::withSeparator(
        mPrivate->mCacheDirectory.empty() ? tConfig.mDataDirectory + "cache/" : mPrivate->mCacheDirectory);
    tConfig.mDefaults.mTerrainCacheMb = mPrivate->mTerrainCacheMb;
    tConfig.mDefaults.mPrefetchTiles = mPrivate->mPrefetchTiles;
    std::error_code tError;
    std::filesystem::create_directories(tConfig.mDataDirectory, tError);
    std::filesystem::create_directories(tConfig.mCacheDirectory, tError);

    mPrivate->mData = std::make_unique<XMapData>(tConfig);
    mPrivate->mData->start();
    log("Prime Map API {} ready (data: {}, cache: {})", versionString(), tConfig.mDataDirectory,
        tConfig.mCacheDirectory);
    return true;
}

void XMapManager::shutdown()
{
    if (!mPrivate->mData)
    {
        return;
    }
    for (XMap* tMap : mPrivate->mMaps)
    {
        if (tMap->mInstance)
        {
            log("WARNING: shutdown with a live map (id {}); closing it", tMap->getId());
            tMap->mInstance.reset();
        }
    }
    mPrivate->mData->stop();
    mPrivate->mData.reset();
    log("Prime Map API stopped");
}

bool XMapManager::isInitialized() const
{
    return mPrivate->mData != nullptr;
}

Status XMapManager::getStatus() const
{
    Status tStatus;
    if (!mPrivate->mData)
    {
        return tStatus;
    }
    const MapDataStatus& tData = mPrivate->mData->status();
    tStatus.mNavReady = tData.mNavReady;
    tStatus.mAirportsReady = tData.mAirportsReady;
    tStatus.mAirwaysReady = tData.mAirwaysReady;
    tStatus.mTerrainIndexReady = tData.mTerrainIndexReady;
    tStatus.mTerrainPendingImages = tData.mTerrainPendingImages;
    tStatus.mPrecacheRunning = tData.mPrecacheRunning;
    tStatus.mPrecacheDone = tData.mPrecacheDone;
    tStatus.mPrecacheTotal = tData.mPrecacheTotal;
    tStatus.mText = tData.mText;
    tStatus.mOpenMaps = tData.mOpenMaps;
    tStatus.mTerrainCacheLimitMb = tData.mTerrainCacheLimitMb;
    tStatus.mTextureLimitMb = tData.mTextureLimitMb;
    for (const XMap* tMap : mPrivate->mMaps)
    {
        if (tMap->isDiagnosticsOn())
        {
            tStatus.mPerformanceText = tMap->mInstance->window().performanceText();
            break;
        }
    }
    return tStatus;
}

XMap* XMapManager::addMap(const MapConfig& pConfig)
{
    if (!mPrivate->mData)
    {
        log("WARNING: addMap before initialize");
        return nullptr;
    }
    XMap* tMap = new XMap();
    tMap->mManager = this;
    tMap->mInstance = std::make_unique<XMapInstance>(*mPrivate->mData, mPrivate->mNextMapId++);
    tMap->mInstance->setDataLoadedFlag(&tMap->mIsDataLoaded);
    if (!pConfig.mName.empty())
    {
        tMap->setName(pConfig.mName);
    }
    tMap->setPersistence(pConfig.mPersistence);
    tMap->setStyle(pConfig.mStyle);
    tMap->setGeometry(pConfig.mGeometry);
    tMap->setMode(pConfig.mMode);
    tMap->setIsUiVisible(pConfig.mUiVisible);
    tMap->setIsMouseInputEnabled(pConfig.mMouseInput);
    tMap->setVisible(pConfig.mVisible);
    tMap->setImageCache(pConfig.mImageCache);
    tMap->setMaxRenderFps(pConfig.mMaxRenderFps);
    tMap->setXPlaneFont(pConfig.mXPlaneFont);
    mPrivate->mMaps.push_back(tMap);
    return tMap;
}

void XMapManager::removeMap(XMap* pMap)
{
    if (!pMap)
    {
        return;
    }
    const auto tIt = std::find(mPrivate->mMaps.begin(), mPrivate->mMaps.end(), pMap);
    if (tIt == mPrivate->mMaps.end())
    {
        log("WARNING: removeMap: unknown map");
        return;
    }
    delete pMap;  // ~XMap listeden dusurur, ayarlari yazar, pencereyi kapatir
}

XMap* XMapManager::findMap(int pId) const
{
    for (XMap* tMap : mPrivate->mMaps)
    {
        if (tMap->getId() == pId)
        {
            return tMap;
        }
    }
    return nullptr;
}

XMap* XMapManager::findMap(const std::string& pName) const
{
    for (XMap* tMap : mPrivate->mMaps)
    {
        if (!pName.empty() && tMap->getName() == pName)
        {
            return tMap;
        }
    }
    return nullptr;
}

std::vector<XMap*> XMapManager::getMaps() const
{
    return mPrivate->mMaps;
}

int XMapManager::getOpenMapCount() const
{
    return mPrivate->mData ? mPrivate->mData->openMapCount() : 0;
}

MapManagerSettings XMapManager::getSettings() const
{
    return mPrivate->mData ? mPrivate->mData->managerSettings() : MapManagerSettings{};
}

bool XMapManager::setSettings(const MapManagerSettings& pSettings)
{
    if (!mPrivate->mData)
    {
        log("WARNING: setSettings before initialize");
        return false;
    }
    mPrivate->mData->setManagerSettings(pSettings);
    return true;
}

std::string XMapManager::getDataDirectory() const
{
    return mPrivate->mData ? mPrivate->mData->dataDirectory() : std::string();
}

std::string XMapManager::getSettingsFilePath() const
{
    return mPrivate->mData ? mPrivate->mData->settingsPath() : std::string();
}

std::string XMapManager::getMapsDirectory() const
{
    return mPrivate->mData ? mPrivate->mData->mapsDirectory() : std::string();
}

std::string XMapManager::getCacheDirectory() const
{
    return mPrivate->mData ? mPrivate->mData->cacheDirectory() : std::string();
}

std::optional<MapLayer> XMapManager::parentOf(MapLayer pLayer) const
{
    return LayerRules::parentOf(pLayer);
}

std::vector<MapLayer> XMapManager::childrenOf(MapLayer pLayer) const
{
    return LayerRules::childrenOf(pLayer);
}

std::vector<MapLayer> XMapManager::topLevelLayers() const
{
    return LayerRules::topLevelLayers();
}

LayerKind XMapManager::kindOf(MapLayer pLayer) const
{
    return LayerRules::kindOf(pLayer);
}

const char* XMapManager::layerName(MapLayer pLayer) const
{
    return LayerRules::layerName(pLayer);
}

const char* XMapManager::layerKey(MapLayer pLayer) const
{
    return LayerRules::layerKey(pLayer);
}

std::optional<MapLayer> XMapManager::layerFromKey(std::string_view pKey) const
{
    return LayerRules::layerFromKey(pKey);
}

bool XMapManager::isLayerDefaultVisible(MapLayer pLayer, MapMode pMode) const
{
    return LayerRules::isLayerDefaultVisible(pLayer, pMode);
}

XMapManager* XMapManagerFactory::createXMapManager()
{
    // Her cagri ayri yonetici (ve ayri onbellek) uretir; bir X-Plane surecinde bir tane yeter.
    return new XMapManager();
}

void XMapManagerFactory::destroyXMapManager(XMapManager* pManager)
{
    delete pManager;
}

}  // namespace prime
