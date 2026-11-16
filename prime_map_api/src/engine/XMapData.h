// Haritalarin ORTAK verisi ve her karede ilerleyen ortak isler (surecte tek). Onbellek ortaktir:
// sinirlari acik harita sayisina gore buyur (applyLimits). Kendi flight loop'unu kaydeder.
#pragma once

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/XMapTypes.h"
#include "Settings.h"
#include "data/AircraftData.h"
#include "data/AirportDetailStore.h"
#include "data/AirportGeometryStore.h"
#include "data/AirwayDatabase.h"
#include "data/NavDatabase.h"
#include "data/NavLoader.h"
#include "data/TerrainClasses.h"
#include "data/TerrainStore.h"
#include "layers/TerrainHub.h"
#include "XMapView.h"

namespace prime
{

class XMap;  // ortak veriye baglanan harita nesnesi (api/XMap.h)
}  // namespace prime

struct XMapDataConfig
{
    std::string mDataDirectory;   // sonunda ayirici ile; altinda prime_map.json, maps/
    std::string mCacheDirectory;  // sonunda ayirici ile; altinda terrain/ ve airports/
    // prime_map.json'da karsiligi olmayan anahtarlarin ilk degeri (host'un InitConfig'inden)
    prime::XMapManagerSettings mDefaults;
};

// Yukleme durumu (API Status'un ic karsiligi)
struct XMapDataStatus
{
    bool mNavReady = false;
    bool mAirportsReady = false;
    bool mAirwaysReady = false;
    bool mTerrainIndexReady = false;
    int mTerrainPendingImages = 0;  // butun gorunur haritalarin toplami
    bool mPrecacheRunning = false;
    int mPrecacheDone = 0;
    int mPrecacheTotal = 0;
    std::string mText;              // ortak durum + toplam bekleyen zemin
    int mOpenMaps = 0;
    int mTerrainCacheLimitMb = 0;   // su an gecerli disk siniri (0 = sinirsiz)
    int mTextureLimitMb = 0;        // su an gecerli doku butcesi
};

class XMapData
{
public:
    explicit XMapData(const XMapDataConfig& pConfig);
    ~XMapData();
    XMapData(const XMapData&) = delete;
    XMapData& operator=(const XMapData&) = delete;
    void start();  // merkezi ayarlar, palet, flight loop (Init)
    void stop();   // isciler durur (Shutdown)
    bool isRunning() const { return mRunning; }  // Start ile Stop arasinda true

    // --- Haritalar ---
    void addInstance(prime::XMap* pInstance);     // harita nesnesi olustu (henuz acik olmayabilir)
    void removeInstance(prime::XMap* pInstance);  // harita nesnesi yok oluyor
    void openXMapsChanged();                       // bir harita acildi / kapandi: sinirlari yeniden hesapla
    int openXMapCount() const;

    // --- Ortak depolar (katmanlar bunlara bakar) ---
    TerrainHub& hub();
    TerrainStore& terrain();
    NavDatabase& navs();
    AirwayDatabase& airways();
    AirportGeometryStore& airportGeometry();
    AirportDetailStore& airportDetails();
    const AirportGeometryStore& airports() const;
    const XMapDataStatus& status() const;

    // --- Zemin paletleri (harita basina ayar) ---
    // Paleti hazirlar: [palette] source = xplane ise X-Plane dokularindan turetilen renkler uygulanir;
    // ayni icerikli palet daha once tanitildiysa onun numarasi doner, yoksa depoya eklenir.
    int registerPalette(TerrainPalette* pPalette);

    // --- Merkezi ayarlar (prime_map.json) ---
    const prime::XMapManagerSettings& managerSettings() const;
    void setManagerSettings(const prime::XMapManagerSettings& pSettings);  // uygular + dosyaya yazar
    std::string settingsPath() const;        // <veri>/prime_map.json
    const std::string& dataDirectory() const;
    const std::string& cacheDirectory() const;
    std::string mapsDirectory() const;       // <veri>/maps/
    // Bolge onbellegi: alanin kurulu DSF karolarini diske hazirlar. Baslamazsa false (neden: durum metni)
    bool startPrecache(const GeoBounds& pBounds);
    void cancelPrecache();
    // Veri yukleniyor mu (harita resmi bu surece hiz sinirinda yenilenir)
    bool isBusy() const;
    // Ortak durum metni (yukleme, bildirim); haritalar kendi "Loading terrain" yazisini ekler
    const std::string& sharedStatusText() const;
    const std::string& terrainWorkerStatus() const;
    bool isTerrainIdle() const;
    void showNotice(const std::string& pText);

private:
    static float flightLoopCallback(float pSinceLastCall, float pSinceLastLoop, int pCounter, void* pRefcon);
    float onFlightLoop();
    void stepLoading();
    void updateStatus();
    void updatePrefetch(float pNow);
    void loadLibrarySettings();
    bool saveLibrarySettings() const;
    void applyLimits();  // acik harita sayisina gore disk siniri, doku butcesi, bellek olcegi
    static void logReport(const std::string& pReport);

    XMapDataConfig mConfig;
    prime::XMapManagerSettings mSettings;
    NavDatabase mNavDatabase;
    NavLoader mNavLoader{mNavDatabase};  // mNavDatabase'den sonra tanimli olmali
    std::unique_ptr<AirportGeometryStore> mAirportGeometry;
    std::unique_ptr<AirportDetailStore> mAirportDetails;  // mAirportGeometry'ye bagli
    bool mLoadStarted = false;
    AirwayDatabase mAirways;
    TerrainPalette mDefaultPalette;  // palet 0: varsayilan zemin ayarlari
    TerrainStore mTerrainStore;
    TerrainHub mHub{mTerrainStore};  // mTerrainStore'dan sonra tanimli olmali
    std::string mXPlaneDirectory;
    // X-Plane dokularindan turetilen sinif renkleri (bir kez hesaplanir / onbellekten okunur)
    bool mDerivedLoaded = false;
    bool mDerivedOk = false;
    std::array<TerrainColor, kTerrainClassCount> mDerivedColors = {};
    std::array<int, kTerrainClassCount> mDerivedCounts = {};
    std::map<std::string, int> mPaletteIds;  // palet metni -> TerrainStore numarasi
    std::vector<prime::XMap*> mInstances;  // yasayan harita nesneleri (acik ya da degil)
    float mLastPrefetch = -100.0f;
    bool mPrecacheRunning = false;
    std::string mNotice;
    float mNoticeUntil = 0.0f;
    bool mFlightLoopRegistered = false;
    bool mRunning = false;  // Start/Stop arasinda true
    bool mBusy = false;
    bool mTerrainIdle = true;
    std::string mSharedText;
    std::string mTerrainWorkerText;
    XMapDataStatus mStatus;
};
