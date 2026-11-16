// Prime Map API: X-Plane 11 haritasini baska bir eklentinin icine gomer (SHARED DLL).
// Tum metotlar X-Plane ana thread'inden cagrilmalir; gecersiz deger no-op + Log uyarisi, false / bos doner.
#ifndef PRIME_MAP_H
#define PRIME_MAP_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "primemapglobal.h"

// Motor tipleri (global ad alani): engine/MapInstance.h, api/XMapManagerPrivate.h
class XMapInstance;
struct XMapManagerPrivate;

namespace prime
{

constexpr int kVersionMajor = 0;
constexpr int kVersionMinor = 24;
constexpr int kVersionPatch = 0;
PRIME_MAP_EXPORT std::string versionString();  // "0.24.0"

struct LatLon
{
    double mLat = 0.0;  // derece, kuzey +
    double mLon = 0.0;  // derece, dogu +
};

struct GeoBounds
{
    double mSouth = 0.0;
    double mWest = 0.0;
    double mNorth = 0.0;
    double mEast = 0.0;
};

// X-Plane global ekran birimi (boxel). mTop: ust kenarin y'si (y yukari artar).
struct Geometry
{
    int mLeft = 0;
    int mTop = 0;
    int mWidth = 0;
    int mHeight = 0;
};

enum class MapMode
{
    Scenery,
    Vfr,
    IfrLow,
    IfrHigh,
    Dark,
};

enum class WindowStyle
{
    Embedded,    // cercevesiz; tasiyamaz / boyutlandiramaz
    Standalone,  // X-Plane cercevesi; kullanici tasir, pop-out
};

enum class DrawTool
{
    Pan,
    Point,
    Route,
    Region,
};

enum class DataSource
{
    XPlane,  // dataref'ler / FMS
    Host,    // setAircraft / setRoute ile host verir
};

// Haritanin ayar dosyalari (maps/<ad>/) nasil kullanilsin
enum class Persistence
{
    Off,          // dosya yok
    SaveOnly,     // okunmaz, yazilir (kod her acilista kazanir)
    LoadAndSave,  // varsayilan: initialize() dosyalari okur (son oturum geri gelir), degisince yazar
};

struct Status
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
    std::string mPerformanceText;
    int mOpenMaps = 0;
    int mTerrainCacheLimitMb = 0;
    int mTextureLimitMb = 0;
};

enum class MapLayer
{
    MapGroup, Terrain, TerrainForests, TerrainBuiltUp,
    Roads, RoadsMotorways, RoadsPrimary, RoadsSecondary, RoadsLocal, RoadsRailways, RoadsPowerLines,
    Grid,
    AirportsGroup, AirportSymbols,
    Runways, RunwaysPaved, RunwaysUnpaved, RunwaysWater, RunwaysOverruns, RunwaysMarkings, RunwaysNumbers,
    Surfaces, SurfacesPavement, SurfacesPavementEdges, SurfacesBoundary, SurfacesMarkings,
    Objects, ObjectsLighting, ObjectsHelipads, ObjectsBeacons, ObjectsWindsocks, ObjectsGates, ObjectsTieDowns,
    ObjectsHangars, ObjectsOtherStarts, ObjectsStartNames, ObjectsApproachLights, ObjectsSigns,
    TaxiRoutes,
    NavaidsGroup, Vor, Ndb, Ils, Fixes,
    AirwaysGroup, AirwaysLow, AirwaysHigh,
    FlightGroup, Aircraft, FmsRoute, FlightTrack,
    Drawings, DrawingsPoints, DrawingsRoutes, DrawingsRegions, DrawingsLabels,
    Count
};

enum class LayerKind
{
    Group,
    Layer,
    SubLayer,
};

enum class LayerState
{
    Off,
    On,
    Mixed,
};

struct AirportMatch
{
    std::string mIdent;
    std::string mName;
    LatLon mPosition;
};

struct MapObject
{
    MapLayer mLayer = MapLayer::Count;
    std::string mText;
};

enum class ShapeKind
{
    Point,
    Route,
    Region,
};

struct Shape
{
    int mId = 0;
    ShapeKind mKind = ShapeKind::Point;
    std::string mLabel;
    std::vector<LatLon> mPoints;
};

struct AircraftState
{
    bool mValid = false;
    LatLon mPosition;
    double mHeadingDeg = 0.0;
    double mGroundSpeedKt = 0.0;
    double mAltitudeFt = 0.0;
};

struct RouteWaypoint
{
    std::string mIdent;
    LatLon mPosition;
    int mAltitudeFt = 0;
};

struct Rgb
{
    int mR = 0;
    int mG = 0;
    int mB = 0;
};

struct TerrainSettings
{
    bool mUseXPlaneColors = true;
    std::vector<std::pair<std::string, Rgb>> mClassColors;  // "water", "forest_conifer" ...
    double mShadingStrength = 0.0;
    double mBlendMeters = 0.0;
    double mBlendMaxPixels = 0.0;
    double mWarpMeters = 0.0;
    double mWarpWavelength = 0.0;
    double mCloseupClassMix = 0.0;
    bool mReliefMultiDirectional = true;
    double mReliefExaggeration = 0.0;
    double mReliefCoarseBoost = 0.0;
    double mReliefSlopeTone = 0.0;
    double mReliefFlatSlope = 0.0;
    bool mCoastline = true;
    double mCoastlineStrength = 0.0;
    double mWaterBandMeters = 0.0;
    double mWaterBandMaxPixels = 0.0;
    double mWaterBandStrength = 0.0;
    double mDepthTone = 0.0;
    double mDepthFullMeters = 0.0;
    bool mVfrBands = true;
    bool mVfrContourLines = false;
    double mVfrContourStrength = 0.0;
    std::vector<std::pair<double, Rgb>> mVfrBandsFeet;
};

// addMap()'e verilen harita yapilandirmasi; her alan sonradan da ayarlanabilir
struct MapConfig
{
    std::string mName;               // ayar klasoru maps/<ad>/; bos = map_<kimlik>
    Persistence mPersistence = Persistence::LoadAndSave;
    Geometry mGeometry;
    WindowStyle mStyle = WindowStyle::Embedded;
    MapMode mMode = MapMode::Scenery;
    bool mUiVisible = false;
    bool mMouseInput = false;
    bool mVisible = true;
    bool mImageCache = true;
    int mMaxRenderFps = 15;
    bool mXPlaneFont = false;
};

class XMapManager;

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4251)
#endif

class PRIME_MAP_EXPORT XMap
{
public:
    bool initialize();            // pencereyi acar, veri yuklemesi arka planda surer
    void close();                 // ayarlari yazar, pencereyi kapatir (nesne ve ayarlari kalir)
    bool isInitialized() const;
    bool isDataLoaded() const;    // nav + havalimani + zemin indeksi hazir, bekleyen doku yok
    void onRender();              // kare cizimi; veri yuklenirken "loading" gorunur

    // --- Kimlik ve dosyalar ---
    int getId() const;
    bool setName(const std::string& pName);
    std::string getName() const;
    std::string getSettingsDirectory() const;
    void setPersistence(Persistence pPersistence);
    Persistence getPersistence() const;
    bool saveSettings();

    // --- Pencere ---
    void setStyle(WindowStyle pStyle);  // yalnizca kapaliyken
    WindowStyle getStyle() const;
    void setTitle(const std::string& pTitle);
    std::string getTitle() const;
    void setGeometry(const Geometry& pGeometry);
    Geometry getGeometry() const;
    void setVisible(bool pVisible);
    bool isVisible() const;
    void bringToFront();
    void setPoppedOut(bool pPoppedOut);
    bool isPoppedOut() const;
    void setIsUiVisible(bool pVisible);
    bool isUiVisible() const;
    void setIsMouseInputEnabled(bool pEnabled);
    bool isMouseInputEnabled() const;

    // --- Gorunum ---
    void setCenter(double pLat, double pLon);
    LatLon getCenter() const;
    void setZoom(double pZoom);
    double getZoom() const;
    static double minZoom();
    static double maxZoom();
    static double hardMaxZoom();
    bool setZoomLimits(double pMinZoom, double pMaxZoom);
    double getMinZoomLimit() const;
    double getMaxZoomLimit() const;
    void zoomIn();
    void zoomOut();
    void setZoomStep(double pStep);
    double getZoomStep() const;
    void panLeft();
    void panRight();
    void panUp();
    void panDown();
    void setPanStep(double pFractionOfView);
    double getPanStep() const;
    void panBy(double pDeltaX, double pDeltaY);
    void fitBounds(const GeoBounds& pBounds);
    GeoBounds getVisibleBounds() const;
    double getMetersPerBoxel() const;
    void setFollowAircraft(bool pFollow);
    bool isFollowingAircraft() const;
    void centerOnAircraft();
    bool goToAirport(const std::string& pIdent);
    std::vector<AirportMatch> findAirports(const std::string& pQuery, int pMaxResults) const;
    LatLon screenToGeo(int pX, int pY) const;
    bool geoToScreen(double pLat, double pLon, int* pX, int* pY) const;

    // --- Mod ---
    void setMode(MapMode pMode);
    MapMode getMode() const;
    void nextMode();

    // --- Katmanlar ---
    void setIsLayerVisible(MapLayer pLayer, bool pVisible);
    bool isLayerVisible(MapLayer pLayer) const;
    void toggleLayer(MapLayer pLayer);
    void setLayersVisible(std::initializer_list<MapLayer> pLayers, bool pVisible);
    void showOnlyLayers(std::initializer_list<MapLayer> pLayers);
    void resetLayersToDefaults();
    LayerState getLayerState(MapLayer pLayer) const;
    bool setLayerZoomRange(MapLayer pLayer, double pMinZoom, double pMaxZoom);
    void getLayerZoomRange(MapLayer pLayer, double* pMinZoom, double* pMaxZoom) const;
    void clearLayerZoomRange(MapLayer pLayer);
    void clearAllLayerZoomRanges();
    bool setColor(MapLayer pLayer, const Rgb& pColor);  // katman cizim rengi (dosyada [layer_colors])
    Rgb getColor(MapLayer pLayer) const;                 // override yoksa mR < 0 doner
    void clearColor(MapLayer pLayer);
    void clearAllColors();

    // --- Cizimler ---
    void setDrawTool(DrawTool pTool);
    DrawTool getDrawTool() const;
    bool addDrawingPoint(double pLat, double pLon);
    bool finishDrawing();
    bool undoDrawing();
    void clearDrawings();
    bool hasUnfinishedDrawing() const;
    std::vector<Shape> getShapes() const;
    bool removeShape(int pId);
    std::string exportDrawings() const;
    bool importDrawings(const std::string& pText);

    // --- Ucak, iz, rota ---
    void setAircraftSource(DataSource pSource);
    DataSource getAircraftSource() const;
    void setAircraft(const AircraftState& pState);
    AircraftState getAircraft() const;
    void clearTrack();
    void setRouteSource(DataSource pSource);
    DataSource getRouteSource() const;
    void setRoute(const std::vector<RouteWaypoint>& pWaypoints, int pActiveIndex);

    // --- Sorgu ---
    std::vector<MapObject> queryAt(double pLat, double pLon) const;
    std::vector<MapObject> queryAtScreen(int pX, int pY) const;

    // --- Cizim secenekleri ---
    void setImageCache(bool pEnabled);
    bool isImageCacheEnabled() const;
    void setMaxRenderFps(int pFps);
    int getMaxRenderFps() const;
    void setXPlaneFont(bool pXPlaneFont);
    bool isXPlaneFont() const;

    // --- Ayarlar (metin olarak disa / ice aktarma) ---
    std::string exportSettings() const;
    bool importSettings(const std::string& pText);
    TerrainSettings getTerrainSettings() const;
    void setTerrainSettings(const TerrainSettings& pSettings);
    std::string exportTerrainSettings() const;
    bool importTerrainSettings(const std::string& pText);
    uint64_t getSettingsRevision() const;
    uint64_t getDrawingsRevision() const;

    // --- Durum ---
    std::string getStatusText() const;
    int getTerrainPendingImages() const;

    // --- Zemin on onbellegi ve teshis ---
    bool precacheVisibleArea();
    bool precacheArea(const GeoBounds& pArea);
    void cancelPrecache();
    void setDiagnostics(bool pOn);
    bool isDiagnosticsOn() const;

private:
    friend class XMapManager;
    XMap();
    ~XMap();

    XMap(const XMap&) = delete;
    XMap& operator=(const XMap&) = delete;

    std::unique_ptr<::XMapInstance> mInstance;  // shutdown sonrasi nullptr olabilir
    std::atomic<bool> mIsDataLoaded{false};
    XMapManager* mManager = nullptr;  // sahibi; yok olurken kendini listeden dusurur
};

// Merkezi ayarlar (prime_map.ini): butun haritalarin ORTAK onbellek butceleri.
// Sinir = taban x (1 + yuzde x (acik harita - 1) / 100); tavani asamaz.
struct MapManagerSettings
{
    int mTerrainCacheMb = 1024;
    int mCacheGrowthPercentPerMap = 25;
    int mTerrainCacheMaxMb = 0;
    int mPrefetchTiles = 20;
    int mTextureMemoryMb = 512;
    int mMemoryGrowthPercentPerMap = 50;
    int mTextureMemoryMaxMb = 2048;
    bool mAutoSave = true;
    double mAutoSaveSeconds = 2.0;
};

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

    XMap* addMap(const MapConfig& pConfig = {});
    void removeMap(XMap* pMap);  // ayarlari yazar, nesneyi yok eder
    XMap* findMap(int pId) const;                   // yoksa nullptr
    XMap* findMap(const std::string& pName) const;  // setName ile verilen adla
    std::vector<XMap*> getMaps() const;
    int getOpenMapCount() const;

    MapManagerSettings getSettings() const;
    bool setSettings(const MapManagerSettings& pSettings);

    std::string getDataDirectory() const;
    std::string getSettingsFilePath() const;
    std::string getMapsDirectory() const;
    std::string getCacheDirectory() const;

    // Katman hiyerarsisi (panel sirasi ve kurallariyla)
    std::optional<MapLayer> parentOf(MapLayer pLayer) const;
    std::vector<MapLayer> childrenOf(MapLayer pLayer) const;
    std::vector<MapLayer> topLevelLayers() const;
    LayerKind kindOf(MapLayer pLayer) const;
    const char* layerName(MapLayer pLayer) const;
    const char* layerKey(MapLayer pLayer) const;
    std::optional<MapLayer> layerFromKey(std::string_view pKey) const;
    bool isLayerDefaultVisible(MapLayer pLayer, MapMode pMode) const;

private:
    XMapManager();
    XMapManager(const XMapManager&) = delete;
    XMapManager& operator=(const XMapManager&) = delete;

    std::unique_ptr<XMapManagerPrivate> mPrivate;

    friend class XMap;  // xmap, yok olurken kendini listeden dusurur
    friend class XMapManagerFactory;  // ozel kurucu/yikici yalnizca fabrikada
};

class PRIME_MAP_EXPORT XMapManagerFactory
{
public:
    static XMapManager* createXMapManager();  // her cagri yeni yonetici (ve ayri onbellek) uretir
    static void destroyXMapManager(XMapManager* pManager);

private:
    virtual ~XMapManagerFactory() = default;
    explicit XMapManagerFactory() = default;
};

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

}  // namespace prime

#endif  // PRIME_MAP_H
