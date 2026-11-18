// Haritalarin sahibi olan yonetici: klasorler, ortak veri, harita ekleme ve kaldirma.
#ifndef PRIME_MAP_SRC_API_XMAPMANAGER_H
#define PRIME_MAP_SRC_API_XMAPMANAGER_H

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "XMapTypes.h"

class XMapData;

namespace prime
{
class XMap;

class PRIME_MAP_EXPORT XMapManager
{
public:
    ~XMapManager();

    void setDataDirectory(const std::string& pPath);
    void setCacheDirectory(const std::string& pPath);
    void setLogPrefix(const std::string& pPrefix);
    void setLogCallback(std::function<void(std::string_view pLine)> pCallback);
    void setTerrainCacheMb(int pMb);
    void setPrefetchTiles(int pTiles);

    bool initialize();
    void shutdown();

    bool isInitialized() const;
    Status getStatus() const;

    XMap* addXMap(const XMapConfig& pConfig = {});
    void removeXMap(XMap* pXMap);
    XMap* findXMap(int pId) const;
    XMap* findXMap(const std::string& pName) const;
    std::vector<XMap*> getXMaps() const;
    int getOpenXMapCount() const;

    XMapManagerSettings getSettings() const;
    bool setSettings(const XMapManagerSettings& pSettings);

    std::string getDataDirectory() const;
    std::string getSettingsFilePath() const;
    std::string getXMapsDirectory() const;
    std::string getCacheDirectory() const;

    std::optional<XMapLayer> parentOf(XMapLayer pLayer) const;
    std::vector<XMapLayer> childrenOf(XMapLayer pLayer) const;
    std::vector<XMapLayer> topLevelLayers() const;
    LayerKind kindOf(XMapLayer pLayer) const;

    const char* layerName(XMapLayer pLayer) const;
    const char* layerKey(XMapLayer pLayer) const;

    std::optional<XMapLayer> layerFromKey(std::string_view pKey) const;
    bool isLayerDefaultVisible(XMapLayer pLayer, XMapMode pMode) const;

private:
    XMapManager();
    XMapManager(const XMapManager&) = delete;
    XMapManager& operator=(const XMapManager&) = delete;

    std::string resolvedDataDirectory() const;
    std::string resolvedCacheDirectory() const;

    std::unique_ptr<::XMapData> mData;
    std::vector<XMap*> mXMaps;
    int mNextXMapId = 1;

    std::string mDataDirectory;
    std::string mCacheDirectory;
    std::string mLogPrefix;
    std::function<void(std::string_view)> mLogCallback;
    int mTerrainCacheMb = -1;
    int mPrefetchTiles = -1;

    friend class XMap;
    friend class XMapManagerFactory;
};
}

#endif
