// MapData.h
// Haritalarin ORTAK verisi ve her karede ilerleyen ortak isleri:
//
//   nav verisi (ana thread'de parca parca), apt.dat indeksi + ayrintilari, hava yollari,
//   zemin (DSF -> doku, arka plan iscisi + TerrainHub), onceden yukleme, bolge onbellegi,
//   merkezi ayarlar (prime_map.ini) ve onlardan hesaplanan sinirlar.
//
// 0.23 (coklu harita): harita basina olan her sey (katmanlar, cizimler, ucak kaynagi,
// gorunum, zemin ayarlari, ayar dosyalari) engine/MapInstance'a tasindi:
//
//   MapData (surecte tek)                      MapInstance (harita basina)
//   ---------------------                      ---------------------------
//   NavDatabase, AirportGeometryStore,   <---  LayerManager + katmanlar (depolara bakar)
//   AirportDetailStore, AirwayDatabase,        Drawings, AircraftData, MapWindow
//   TerrainStore + TerrainHub (onbellek)       TerrainPalette (palet numarasi), ayar dosyalari
//
// Onbellek ORTAKTIR: disk onbellegi (cache/terrain, cache/airports), bellekteki karolar ve
// video bellegindeki dokular butun haritalarca paylasilir. Sinirlar acik harita sayisina gore
// buyur (ApplyLimits):
//
//   sinir(n) = taban x (1 + artis% x (n - 1) / 100), tavan varsa onu gecmez      n = acik harita
//
// Kendi flight loop'unu kaydeder (refcon = this) ve her karede haritalari ilerletir.
// XPLM komutu / menusu YOK: onlar sadece host'ta.
#pragma once

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "PrimeMap.h"
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
#include "MapView.h"

class MapInstance;

struct MapDataConfig
{
    std::string mDataDirectory;   // sonunda ayirici ile; altinda prime_map.ini, maps/
    std::string mCacheDirectory;  // sonunda ayirici ile; altinda terrain/ ve airports/
    // prime_map.ini'de karsiligi olmayan anahtarlarin ilk degeri (host'un InitConfig'inden)
    prime::LibrarySettings mDefaults;
};

// Yukleme durumu (API Status'un ic karsiligi)
struct MapDataStatus
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

class MapData
{
public:
    explicit MapData(const MapDataConfig& pConfig);
    ~MapData();

    MapData(const MapData&) = delete;
    MapData& operator=(const MapData&) = delete;

    void Start();  // merkezi ayarlar, palet, flight loop (Init)
    void Stop();   // isciler durur (Shutdown)
    bool IsRunning() const { return mRunning; }  // Start ile Stop arasinda true

    // --- Haritalar ---
    void AddInstance(MapInstance* pInstance);     // harita nesnesi olustu (henuz acik olmayabilir)
    void RemoveInstance(MapInstance* pInstance);  // harita nesnesi yok oluyor
    void OpenMapsChanged();                       // bir harita acildi / kapandi: sinirlari yeniden hesapla
    int OpenMapCount() const;

    // --- Ortak depolar (katmanlar bunlara bakar) ---
    TerrainHub& Hub();
    TerrainStore& Terrain();
    NavDatabase& Navs();
    AirwayDatabase& Airways();
    AirportGeometryStore& AirportGeometry();
    AirportDetailStore& AirportDetails();
    const AirportGeometryStore& Airports() const;
    const MapDataStatus& Status() const;

    // --- Zemin paletleri (harita basina ayar) ---
    // Paleti hazirlar: [palette] source = xplane ise X-Plane dokularindan turetilen renkler uygulanir;
    // ayni icerikli palet daha once tanitildiysa onun numarasi doner, yoksa depoya eklenir.
    int RegisterPalette(TerrainPalette* pPalette);
    // terrain_colors.ini basindaki aciklama (butun haritalar icin ayni)
    static const char* TerrainSettingsComment();

    // --- Merkezi ayarlar (prime_map.ini) ---
    const prime::LibrarySettings& LibrarySettings() const;
    void SetLibrarySettings(const prime::LibrarySettings& pSettings);  // uygular + dosyaya yazar
    std::string SettingsPath() const;        // <veri>/prime_map.ini
    const std::string& DataDirectory() const;
    const std::string& CacheDirectory() const;
    std::string MapsDirectory() const;       // <veri>/maps/

    // Bolge onbellegi: alanin kurulu DSF karolarini diske hazirlar. Baslamazsa false (neden: durum metni)
    bool StartPrecache(const GeoBounds& pBounds);
    void CancelPrecache();

    // Veri yukleniyor mu (harita resmi bu surece hiz sinirinda yenilenir)
    bool IsBusy() const;
    // Ortak durum metni (yukleme, bildirim); haritalar kendi "Loading terrain" yazisini ekler
    const std::string& SharedStatusText() const;
    const std::string& TerrainWorkerStatus() const;
    bool IsTerrainIdle() const;
    void ShowNotice(const std::string& pText);

private:
    static float FlightLoopCallback(float pSinceLastCall, float pSinceLastLoop, int pCounter, void* pRefcon);
    float OnFlightLoop();
    void StepLoading();
    void UpdateStatus();
    void UpdatePrefetch(float pNow);
    void LoadLibrarySettings();
    bool SaveLibrarySettings() const;
    void ApplyLimits();  // acik harita sayisina gore disk siniri, doku butcesi, bellek olcegi
    static void LogReport(const std::string& pReport);

    MapDataConfig mConfig;
    prime::LibrarySettings mSettings;
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

    std::vector<MapInstance*> mInstances;  // yasayan harita nesneleri (acik ya da degil)
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
    MapDataStatus mStatus;
};
