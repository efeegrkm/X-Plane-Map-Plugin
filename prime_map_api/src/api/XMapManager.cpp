// prime::XMapManager uygulamasi: haritalarin sahibi. Veri klasoru bos birakilirsa
// %LOCALAPPDATA%/prime/prime-xmap kullanilir.
#include "api/XMapManager.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <format>

#include "XPLMUtilities.h"

#include "Log.h"
#include "XMapApiUtil.h"
#include "api/XMap.h"
#include "engine/LayerTable.h"
#include "engine/XMapData.h"

namespace
{

const char* kSettingsFileName = "prime_map.json";  // XMapData::settingsPath ile ayni
const char* kXMapsFolder = "maps/";                 // XMapData::mapsDirectory ile ayni

}  // namespace

namespace prime
{

std::string versionString()
{
    return std::format("{}.{}.{}", kVersionMajor, kVersionMinor, kVersionPatch);
}

namespace
{

// Surec ici yonetici kaydi: ayni klasore cozulen ikinci yoneticinin tespiti icin. Kilit dosyasi
// YOK; sadece bu surecteki canli yoneticiler tutulur (butun cagilar X-Plane ana thread'inden).
struct LiveManager
{
    const XMapManager* mManager;
    std::string mData;
    std::string mCache;
};

std::vector<LiveManager>& liveManagers()
{
    static std::vector<LiveManager> tList;
    return tList;
}

}  // namespace

XMapManager::XMapManager() = default;

XMapManager::~XMapManager()
{
    // Kayit temizligi: shutdown cagrilmamis olsa da bu yonetici artik canli degil
    auto& tLive = liveManagers();
    std::erase_if(tLive,
                  [this](const LiveManager& pEntry)
                  {
                      return pEntry.mManager == this;
                  });
    // Kalan haritalar: ayarlari yazilir, pencereleri kapanir, nesneleri silinir
    while (!mXMaps.empty())
    {
        delete mXMaps.front();
    }
}

void XMapManager::setDataDirectory(const std::string& pPath)
{
    mDataDirectory = pPath;
}

void XMapManager::setCacheDirectory(const std::string& pPath)
{
    mCacheDirectory = pPath;
}

void XMapManager::setLogPrefix(const std::string& pPrefix)
{
    mLogPrefix = pPrefix;
    ::setLogPrefix(pPrefix);
}

void XMapManager::setLogCallback(std::function<void(std::string_view pLine)> pCallback)
{
    mLogCallback = pCallback;
    ::setLogCallback(std::move(pCallback));
}

void XMapManager::setTerrainCacheMb(int pMb)
{
    mTerrainCacheMb = std::max(0, pMb);
}

void XMapManager::setPrefetchTiles(int pTiles)
{
    mPrefetchTiles = std::max(0, pTiles);
}

bool XMapManager::initialize()
{
    if (mData)
    {
        return true;  // zaten calisiyor; ayni yoneticiyi iki kez baslatmak anlamsiz
    }
    setLogPrefix(mLogPrefix);
    setLogCallback(mLogCallback);
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);

    XMapDataConfig tConfig;
    tConfig.mDataDirectory = resolvedDataDirectory();
    tConfig.mCacheDirectory = resolvedCacheDirectory();
    // Host vermediyse (-1) XMapManagerSettings varsayilani kalir; 0 gecerli bir degerdir (sinirsiz / kapali)
    if (mTerrainCacheMb >= 0)
    {
        tConfig.mDefaults.mTerrainCacheMb = mTerrainCacheMb;
    }
    if (mPrefetchTiles >= 0)
    {
        tConfig.mDefaults.mPrefetchTiles = mPrefetchTiles;
    }
    std::error_code tError;
    std::filesystem::create_directories(tConfig.mDataDirectory, tError);
    std::filesystem::create_directories(tConfig.mCacheDirectory, tError);

    // Ayni klasore cozulen ikinci yoneticinin surec ici tespiti (kilit dosyasi yok)
    auto& tLive = liveManagers();
    const bool tSameData = std::any_of(tLive.begin(), tLive.end(),
                                       [&tConfig](const LiveManager& pEntry)
                                       {
                                           return pEntry.mData == tConfig.mDataDirectory;
                                       });
    if (tSameData)
    {
        // AYNI VERI KLASORU: gercek cakisma; prime_map.json ve ayni adli maps/<ad> ayarlari ezilir
        log("WARNING: a second manager in this process uses the same data directory ({}): settings and "
            "same-named maps overwrite each other; give each manager its own data directory "
            "(setDataDirectory) or use distinct map names",
            tConfig.mDataDirectory);
    }
    else
    {
        const bool tSameCache = std::any_of(tLive.begin(), tLive.end(),
                                            [&tConfig](const LiveManager& pEntry)
                                            {
                                                return pEntry.mCache == tConfig.mCacheDirectory;
                                            });
        if (tSameCache)
        {
            // Sadece ayni ONBELLEK klasoru: bilincli kurulum (ayarlar ayri, karolar ortak). Guvenli:
            // butun onbellek yazimlari atomik (writeFileAtomic), okumada anahtar dogrulanir, yarim
            // dosya gorunmez; CacheTrim yarismasi kayip dosyayi eksik olarak gorur ve yeniden uretir.
            log("note: a second manager in this process shares the terrain cache directory ({}); "
                "settings stay isolated, tiles are shared",
                tConfig.mCacheDirectory);
        }
    }
    tLive.emplace_back(LiveManager{this, tConfig.mDataDirectory, tConfig.mCacheDirectory});

    mData = std::make_unique<XMapData>(tConfig);
    mData->start();
    log("Prime Map API {} ready (data: {}, cache: {})", versionString(), tConfig.mDataDirectory,
        tConfig.mCacheDirectory);
    return true;
}

void XMapManager::shutdown()
{
    if (!mData)
    {
        return;
    }
    for (XMap* tXMap : mXMaps)
    {
        if (tXMap->mLayers)
        {
            log("WARNING: shutdown with a live map (id {}); closing it", tXMap->getId());
            tXMap->teardown();  // ayarlari yazar, pencereyi ve motor parcalarini birakir
        }
    }
    mData->stop();
    mData.reset();
    auto& tLive = liveManagers();
    std::erase_if(tLive,
                  [this](const LiveManager& pEntry)
                  {
                      return pEntry.mManager == this;
                  });
    log("Prime Map API stopped");
}

bool XMapManager::isInitialized() const
{
    return mData != nullptr;
}

Status XMapManager::getStatus() const
{
    Status tStatus;
    if (!mData)
    {
        return tStatus;
    }
    const XMapDataStatus& tData = mData->status();
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
    for (const XMap* tXMap : mXMaps)
    {
        if (tXMap->isDiagnosticsOn() && tXMap->mWindow)
        {
            tStatus.mPerformanceText = tXMap->mWindow->performanceText();
            break;
        }
    }
    return tStatus;
}

XMap* XMapManager::addXMap(const XMapConfig& pConfig)
{
    if (!mData)
    {
        log("WARNING: addXMap before initialize");
        return nullptr;
    }
    XMap* tXMap = new XMap(*mData, mNextXMapId++);
    tXMap->mManager = this;
    if (!pConfig.mName.empty())
    {
        tXMap->setName(pConfig.mName);
    }
    tXMap->setPersistence(pConfig.mPersistence);
    tXMap->setStyle(pConfig.mStyle);
    tXMap->setGeometry(pConfig.mGeometry);
    tXMap->setMode(pConfig.mMode);
    tXMap->setIsUiVisible(pConfig.mUiVisible);
    tXMap->setIsMouseInputEnabled(pConfig.mMouseInput);
    tXMap->setVisible(pConfig.mVisible);
    tXMap->setImageCache(pConfig.mImageCache);
    tXMap->setMaxRenderFps(pConfig.mMaxRenderFps);
    tXMap->setXPlaneFont(pConfig.mXPlaneFont);
    mXMaps.push_back(tXMap);
    return tXMap;
}

void XMapManager::removeXMap(XMap* pXMap)
{
    if (!pXMap)
    {
        return;
    }
    const auto tIt = std::find(mXMaps.begin(), mXMaps.end(), pXMap);
    if (tIt == mXMaps.end())
    {
        log("WARNING: removeXMap: unknown map");
        return;
    }
    delete pXMap;  // ~XMap listeden dusurur, ayarlari yazar, pencereyi kapatir
}

XMap* XMapManager::findXMap(int pId) const
{
    for (XMap* tXMap : mXMaps)
    {
        if (tXMap->getId() == pId)
        {
            return tXMap;
        }
    }
    return nullptr;
}

XMap* XMapManager::findXMap(const std::string& pName) const
{
    for (XMap* tXMap : mXMaps)
    {
        if (!pName.empty() && tXMap->getName() == pName)
        {
            return tXMap;
        }
    }
    return nullptr;
}

std::vector<XMap*> XMapManager::getXMaps() const
{
    return mXMaps;
}

int XMapManager::getOpenXMapCount() const
{
    return mData ? mData->openXMapCount() : 0;
}

XMapManagerSettings XMapManager::getSettings() const
{
    return mData ? mData->managerSettings() : XMapManagerSettings{};
}

bool XMapManager::setSettings(const XMapManagerSettings& pSettings)
{
    if (!mData)
    {
        log("WARNING: setSettings before initialize");
        return false;
    }
    mData->setManagerSettings(pSettings);
    return true;
}

// Yol sorgulari initialize() oncesi de calisir (host dosyalari onceden hazirlayabilsin)
std::string XMapManager::getDataDirectory() const
{
    return mData ? mData->dataDirectory() : resolvedDataDirectory();
}

std::string XMapManager::getSettingsFilePath() const
{
    return mData ? mData->settingsPath() : resolvedDataDirectory() + kSettingsFileName;
}

std::string XMapManager::getXMapsDirectory() const
{
    return mData ? mData->mapsDirectory() : resolvedDataDirectory() + kXMapsFolder;
}

std::string XMapManager::getCacheDirectory() const
{
    return mData ? mData->cacheDirectory() : resolvedCacheDirectory();
}

std::string XMapManager::resolvedDataDirectory() const
{
    std::string tDataDir = mDataDirectory;
    if (tDataDir.empty())
    {
        const char* tAppData = std::getenv("LOCALAPPDATA");
        tDataDir = (tAppData ? std::string(tAppData) : std::string(".")) + "/prime/prime-xmap";
    }
    return api::withSeparator(tDataDir);
}

std::string XMapManager::resolvedCacheDirectory() const
{
    return api::withSeparator(mCacheDirectory.empty() ? resolvedDataDirectory() + "cache/" : mCacheDirectory);
}

std::optional<XMapLayer> XMapManager::parentOf(XMapLayer pLayer) const
{
    return LayerRules::parentOf(pLayer);
}

std::vector<XMapLayer> XMapManager::childrenOf(XMapLayer pLayer) const
{
    return LayerRules::childrenOf(pLayer);
}

std::vector<XMapLayer> XMapManager::topLevelLayers() const
{
    return LayerRules::topLevelLayers();
}

LayerKind XMapManager::kindOf(XMapLayer pLayer) const
{
    return LayerRules::kindOf(pLayer);
}

const char* XMapManager::layerName(XMapLayer pLayer) const
{
    return LayerRules::layerName(pLayer);
}

const char* XMapManager::layerKey(XMapLayer pLayer) const
{
    return LayerRules::layerKey(pLayer);
}

std::optional<XMapLayer> XMapManager::layerFromKey(std::string_view pKey) const
{
    return LayerRules::layerFromKey(pKey);
}

bool XMapManager::isLayerDefaultVisible(XMapLayer pLayer, XMapMode pMode) const
{
    return LayerRules::isLayerDefaultVisible(pLayer, pMode);
}

}  // namespace prime
