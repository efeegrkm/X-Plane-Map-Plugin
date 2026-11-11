// MapData.h
// Haritanin butun verisi ve her karede ilerleyen isleri (0.20'de Plugin.cpp'den ayrildi):
//
//   nav verisi (ana thread'de parca parca), apt.dat indeksi + ayrintilari, hava yollari,
//   zemin (DSF -> doku, arka plan iscisi), zemin paleti, ucak (dataref ya da host),
//   katmanlar (LayerManager), kullanici cizimleri, onceden yukleme, bolge onbellegi.
//
// Kendi flight loop'unu kaydeder (refcon = this). Acik harita pencerelerine (coklu)
// durum metnini ve "veri geliyor" bilgisini verir. XPLM komutu / menusu YOK: onlar
// sadece eklentinin kabugunda (plugin/PluginMain.cpp). Dosya yazmaz; sadece host'un verdigi
// onbellek klasorune (zemin / havalimani onbellegi).
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Drawings.h"
#include "LayerManager.h"
#include "Settings.h"
#include "data/AircraftData.h"
#include "data/AirportDetailStore.h"
#include "data/AirportGeometryStore.h"
#include "data/AirwayDatabase.h"
#include "data/NavDatabase.h"
#include "data/NavLoader.h"
#include "data/TerrainClasses.h"
#include "data/TerrainStore.h"
#include "MapView.h"

class FmsLayer;
class MapWindow;
class TerrainLayer;
class TrackLayer;

struct MapDataConfig
{
    std::string mCacheDirectory;  // sonunda ayirici ile; altinda terrain/ ve airports/
    int mTerrainCacheMb = 1024;
    int mPrefetchTiles = 20;
};

// Yukleme durumu (API Status'un ic karsiligi)
struct MapDataStatus
{
    bool mNavReady = false;
    bool mAirportsReady = false;
    bool mAirwaysReady = false;
    bool mTerrainIndexReady = false;
    int mTerrainPendingImages = 0;
    bool mPrecacheRunning = false;
    int mPrecacheDone = 0;
    int mPrecacheTotal = 0;
    std::string mText;
};

class MapData
{
public:
    explicit MapData(const MapDataConfig& pConfig);
    ~MapData();

    MapData(const MapData&) = delete;
    MapData& operator=(const MapData&) = delete;

    void Start();  // katmanlar, palet, flight loop (Init)
    void Stop();   // isciler durur (Shutdown)

    // Harita penceresi acilinca / kapaninca
    void AddWindow(MapWindow* pWindow);     // harita penceresini cari listesine ekle
void RemoveWindow(MapWindow* pWindow);  // cikar (yoksa hicbir sey yapmaz)

    LayerManager& Layers();
    Drawings& UserDrawings();
    AircraftData& Aircraft();
    FmsLayer* Fms();
    TrackLayer* Track();
    const AirportGeometryStore& Airports() const;
    const MapDataStatus& Status() const;

    // Bolge onbellegi: alanin kurulu DSF karolarini diske hazirlar. Baslamazsa false (neden: durum metni)
    bool StartPrecache(const GeoBounds& pBounds);
    void CancelPrecache();

    // Zemin ayarlari (terrain_colors.ini bicimi). Zemin iscisi ilk karede baslar: ondan once verilen
    // ayar tam etkilidir; sonra verilen bir sonraki X-Plane acilisinda (bugunku kural).
    void ImportTerrainSettings(const std::string& pText);
    std::string ExportTerrainSettings() const;
    const TerrainPalette& Palette() const;

private:
    static float FlightLoopCallback(float pSinceLastCall, float pSinceLastLoop, int pCounter, void* pRefcon);
    float OnFlightLoop();
    void StepLoading();
    void UpdateStatus();
    void UpdatePrefetch(float pNow);
    void ShowNotice(const std::string& pText);
    void CreateLayers();
    void ApplyXPlaneColors();
    static void LogReport(const std::string& pReport);

    MapDataConfig mConfig;
    AircraftData mAircraft;
    NavDatabase mNavDatabase;
    NavLoader mNavLoader{mNavDatabase};  // mNavDatabase'den sonra tanimli olmali
    std::unique_ptr<AirportGeometryStore> mAirportGeometry;
    std::unique_ptr<AirportDetailStore> mAirportDetails;  // mAirportGeometry'ye bagli
    bool mLoadStarted = false;
    AirwayDatabase mAirways;
    TerrainPalette mTerrainPalette;
    TerrainStore mTerrainStore;
    std::string mXPlaneDirectory;
    Drawings mDrawings;
    LayerManager mLayers;  // katmanlar yukaridaki verilere bagli: onlardan SONRA tanimli olmali
    TrackLayer* mTrackLayer = nullptr;      // sahibi mLayers
    FmsLayer* mFmsLayer = nullptr;
    TerrainLayer* mTerrainLayer = nullptr;
    std::vector<MapWindow*> mWindows;  // acik harita pencereleri (coklu host / coklu harita)
    float mLastPrefetch = -100.0f;
    bool mPrecacheRunning = false;
    std::string mNotice;
    float mNoticeUntil = 0.0f;
    bool mFlightLoopRegistered = false;
    MapDataStatus mStatus;
};
