// prime::XMapManager: haritalarin sahibi. initialize() ortak veriyi (XMapData) ve arka plan
// islerini baslatir; addXMap() yeni harita nesnesi verir (acilmaz), removeXMap() yazar ve
// yok eder. Nesneyi yalnizca XMapManagerFactory olusturur / birakir.
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

// C4251 (dll arayuzunde std uyeleri): kutuphane ve host ayni MSVC + /MD ile derlenmek zorunda (README),
// bu yuzden zararsiz; derleyici bayragi /wd4251 ile tum hedeflerde kapatilir.

class PRIME_MAP_EXPORT XMapManager
{
public:
    ~XMapManager();

    // initialize() oncesi; hepsi istege bagli. Bos veri klasoru: %LOCALAPPDATA%/prime/prime-xmap
    void setDataDirectory(const std::string& pPath);
    void setCacheDirectory(const std::string& pPath);
    void setLogPrefix(const std::string& pPrefix);
    void setLogCallback(std::function<void(std::string_view pLine)> pCallback);
    void setTerrainCacheMb(int pMb);
    void setPrefetchTiles(int pTiles);

    bool initialize();  // veri yuklemesi arka planda baslar; zaten calisiyorsa true
    void shutdown();

    bool isInitialized() const;
    Status getStatus() const;

    XMap* addXMap(const XMapConfig& pConfig = {});
    void removeXMap(XMap* pXMap);  // ayarlari yazar, nesneyi yok eder
    XMap* findXMap(int pId) const;                   // yoksa nullptr
    XMap* findXMap(const std::string& pName) const;  // setName ile verilen adla
    std::vector<XMap*> getXMaps() const;
    int getOpenXMapCount() const;

    XMapManagerSettings getSettings() const;
    bool setSettings(const XMapManagerSettings& pSettings);

    // Yollar initialize() oncesi de dogru doner (setDataDirectory / setCacheDirectory ya da varsayilan)
    std::string getDataDirectory() const;
    std::string getSettingsFilePath() const;
    std::string getXMapsDirectory() const;
    std::string getCacheDirectory() const;

    // Katman hiyerarsisi (panel sirasi ve kurallariyla)
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

    std::string resolvedDataDirectory() const;   // setDataDirectory ya da %LOCALAPPDATA%/prime/prime-xmap/
    std::string resolvedCacheDirectory() const;  // setCacheDirectory ya da <veri>/cache/

    std::unique_ptr<::XMapData> mData;  // ortak veri ve arka plan isleri; initialize/shutdown
    std::vector<XMap*> mXMaps;          // yasayan harita nesneleri (olusturulma sirasiyla)
    int mNextXMapId = 1;

    // initialize() oncesi yapilandirma (setXxx ile gelir)
    std::string mDataDirectory;
    std::string mCacheDirectory;
    std::string mLogPrefix;
    std::function<void(std::string_view)> mLogCallback;
    int mTerrainCacheMb = -1;  // -1: verilmedi (XMapManagerSettings varsayilani)
    int mPrefetchTiles = -1;

    friend class XMap;                // xmap, yok olurken kendini listeden dusurur
    friend class XMapManagerFactory;  // ozel kurucu/yikici yalnizca fabrikada
};

}  // namespace prime

#endif  // PRIME_MAP_SRC_API_XMAPMANAGER_H
