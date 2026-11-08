// PrimeMap.h
// Prime Map API: X-Plane 11 haritasini baska bir X-Plane eklentisinin icine gomer.
// TASLAK (tasarim onayi icin, feature/api). Bu dosya kutuphanenin TEK genel basligidir:
// sadece standart C++ tipleri kullanir; XPLM, OpenGL ya da projenin ic basliklari yok.
//
// Yasam dongusu (hepsi X-Plane ana thread'inde):
//
//   XPluginEnable  : PrimeMap::Init(tConfig)           veri yuklemesi baslar (arka planda)
//                    tMap = PrimeMap::Map::Open(...)   harita penceresi (tek harita)
//   ...            : tMap->ZoomIn(), tMap->SetMode(...) ... (host'un dugmeleri)
//   XPluginDisable : tMap.reset()                      pencere kapanir
//                    PrimeMap::Shutdown()              arka plan isleri durur
//
// Harita kendi XPLM penceresinde, kendi flight loop'uyla kendisi cizer. Host sadece
// acar, yerini / boyutunu soyler ve fonksiyonlari kendi dugmelerine baglar.
//
// Koordinatlar: X-Plane "global ekran birimi" (boxel; XPLMGetWindowGeometry ile ayni).
// x saga, y YUKARI artar; Geometry sol-ust kose + genislik / yukseklik verir.
//
// Hata modeli: istisna yok. Gecersiz deger: bir sey yapilmaz, Log'a tek satir uyari,
// donus false / bos.
#pragma once

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace PrimeMap
{

// --- Surum --------------------------------------------------------------------
// API kirilirsa major artar; yeni fonksiyon minor, duzeltme patch.
constexpr int kVersionMajor = 1;
constexpr int kVersionMinor = 0;
constexpr int kVersionPatch = 0;
std::string VersionString();  // "1.0.0"

// --- Basit tipler -------------------------------------------------------------
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
    double mEast = 0.0;  // 180. boylami gecen alanda mEast > 180 olabilir
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
    Embedded,    // cercevesiz; kullanici tasiyamaz / boyutlandiramaz; yer sadece SetGeometry ile
    Standalone,  // bugunku pencere: X-Plane cercevesi, kullanici tasir, pop-out
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
    XPlane,  // dataref'ler / FMS (varsayilan)
    Host,    // host SetAircraft / SetRoute ile verir
};

// --- Baslatma ------------------------------------------------------------------
struct InitConfig
{
    std::string mCacheDirectory;            // zemin / havalimani onbellegi; bos = <host eklenti klasoru>/cache/
    std::string mLogPrefix = "[prime-map]";
    // Verilirse log satirlari X-Plane Log.txt yerine buraya gider (onek eklenmis halde, ana thread)
    std::function<void(std::string_view pLine)> mLogCallback;
    int mTerrainCacheMb = 1024;             // zemin disk onbellegi siniri (0 = sinirsiz); bugun [cache] terrain_mb
    int mPrefetchTiles = 20;                // onceden yukleme karo sayisi (0 = kapali); bugun [cache] prefetch_tiles
};

struct Status
{
    bool mNavReady = false;
    bool mAirportsReady = false;
    bool mAirwaysReady = false;
    bool mTerrainIndexReady = false;
    int mTerrainPendingImages = 0;   // "Loading terrain: N images"
    bool mPrecacheRunning = false;
    int mPrecacheDone = 0;
    int mPrecacheTotal = 0;
    std::string mText;               // alt seritteki birlesik durum metni (bugunku ile ayni); bos = yok
    std::string mPerformanceText;    // teshis modunda "FPS .. draw .. ms"; degilse bos
};

bool Init(const InitConfig& pConfig);  // ikinci kez: false
void Shutdown();                       // once Map kapatilmis olmali
bool IsInitialized();
Status GetStatus();

// --- Katmanlar -----------------------------------------------------------------
// Gruplar, katmanlar ve alt katmanlar tek duz enum; sira katman paneliyle ayni.
enum class MapLayer
{
    // Map
    MapGroup, Terrain, TerrainForests, TerrainBuiltUp,
    Roads, RoadsMotorways, RoadsPrimary, RoadsSecondary, RoadsLocal, RoadsRailways, RoadsPowerLines,
    Grid,
    // Airports
    AirportsGroup, AirportSymbols,
    Runways, RunwaysPaved, RunwaysUnpaved, RunwaysWater, RunwaysOverruns, RunwaysMarkings, RunwaysNumbers,
    Surfaces, SurfacesPavement, SurfacesPavementEdges, SurfacesBoundary, SurfacesMarkings,
    Objects, ObjectsLighting, ObjectsHelipads, ObjectsBeacons, ObjectsWindsocks, ObjectsGates, ObjectsTieDowns,
    ObjectsHangars, ObjectsOtherStarts, ObjectsStartNames, ObjectsApproachLights, ObjectsSigns,
    TaxiRoutes,
    // Navaids
    NavaidsGroup, Vor, Ndb, Ils, Fixes,
    // Airways
    AirwaysGroup, AirwaysLow, AirwaysHigh,
    // Flight
    FlightGroup, Aircraft, FmsRoute, FlightTrack,
    // User drawings (grupsuz katman)
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
    Mixed,  // panelin [-] kutusu: alt ogelerin bir kismi acik
};

std::optional<MapLayer> ParentOf(MapLayer pLayer);  // en ust oge (grup, Drawings): nullopt
std::vector<MapLayer> ChildrenOf(MapLayer pLayer);
std::vector<MapLayer> TopLevelLayers();             // gruplar + grupsuz katmanlar, panel sirasiyla
LayerKind KindOf(MapLayer pLayer);
const char* LayerName(MapLayer pLayer);             // panelde gorunen ad ("Water runways")
const char* LayerKey(MapLayer pLayer);              // ayar anahtari ("runways.water"); grup: "group.airports"
std::optional<MapLayer> LayerFromKey(std::string_view pKey);
bool IsLayerDefaultVisible(MapLayer pLayer, MapMode pMode);

// --- Sorgu / arama sonuclari ---------------------------------------------------------
struct AirportMatch
{
    std::string mIdent;  // "KSEA"
    std::string mName;   // "Seattle-Tacoma Intl"
    LatLon mPosition;
};

struct MapObject
{
    MapLayer mLayer = MapLayer::Count;  // nesnenin katmani
    std::string mText;                  // tooltip metni (bugunku ile ayni)
};

// --- Cizimler --------------------------------------------------------------------
enum class ShapeKind
{
    Point,
    Route,
    Region,
};

struct Shape
{
    int mId = 0;                  // RemoveShape icin (oturum icinde sabit)
    ShapeKind mKind = ShapeKind::Point;
    std::string mLabel;           // "P1", "R2", "A3"
    std::vector<LatLon> mPoints;
};

// --- Ucak ve rota ------------------------------------------------------------------
struct AircraftState
{
    bool mValid = false;      // false: ucak cizilmez, takip calismaz
    LatLon mPosition;
    double mHeadingDeg = 0.0;   // gercek (true) yon
    double mGroundSpeedKt = 0.0;
    double mAltitudeFt = 0.0;   // MSL
};

struct RouteWaypoint
{
    std::string mIdent;
    LatLon mPosition;
    int mAltitudeFt = 0;  // 0 = yazilmaz
};

// --- Zemin ayarlari (terrain_colors.ini'nin butun anahtarlari) ----------------------
struct Rgb
{
    int mR = 0;  // 0..255
    int mG = 0;
    int mB = 0;
};

struct TerrainSettings
{
    bool mUseXPlaneColors = true;     // [palette] source = xplane | ini
    std::vector<std::pair<std::string, Rgb>> mClassColors;  // [colors]: "water", "forest_conifer" ...
    double mShadingStrength = 0.0;    // [shading] strength
    double mBlendMeters = 0.0;        // [blend] meters
    double mBlendMaxPixels = 0.0;     // [blend] max_pixels
    double mWarpMeters = 0.0;         // [warp] meters
    double mWarpWavelength = 0.0;     // [warp] wavelength
    double mCloseupClassMix = 0.0;    // [closeup] class_mix
    // [relief]
    bool mReliefMultiDirectional = true;
    double mReliefExaggeration = 0.0;
    double mReliefCoarseBoost = 0.0;
    double mReliefSlopeTone = 0.0;
    double mReliefFlatSlope = 0.0;
    // [water]
    bool mCoastline = true;
    double mCoastlineStrength = 0.0;
    double mWaterBandMeters = 0.0;
    double mWaterBandMaxPixels = 0.0;
    double mWaterBandStrength = 0.0;
    double mDepthTone = 0.0;
    double mDepthFullMeters = 0.0;
    // [vfr]
    bool mVfrBands = true;
    bool mVfrContourLines = false;
    double mVfrContourStrength = 0.0;
    std::vector<std::pair<double, Rgb>> mVfrBandsFeet;  // band_1..band_8: alt sinir (ft) + renk
};
// Varsayilan degerler: GetTerrainSettings() Init'ten hemen sonra varsayilanlari doner.

// --- Harita ----------------------------------------------------------------------
struct OpenConfig
{
    Geometry mGeometry;
    WindowStyle mStyle = WindowStyle::Embedded;
    MapMode mMode = MapMode::Scenery;
    bool mUiVisible = false;
    bool mMouseInput = false;
    bool mVisible = true;
    // Cizim secenekleri (bugun map_settings.ini [render] / [text])
    bool mImageCache = true;     // harita resmi onbellegi (FBO)
    int mMaxRenderFps = 15;      // harita resmi saniyede en cok bu kadar yeniden cizilir
    bool mXPlaneFont = false;    // true: XPLMDrawString (yedek); false: kendi yazi atlasimiz
};

class Map
{
public:
    // Tek harita: ikinci Open (ilki kapanmadan) nullptr + log. Init yapilmadiysa nullptr.
    static std::unique_ptr<Map> Open(const OpenConfig& pConfig);
    ~Map();  // pencereyi kapatir

    Map(const Map&) = delete;
    Map& operator=(const Map&) = delete;

    // --- Pencere ---
    void SetGeometry(const Geometry& pGeometry);
    Geometry GetGeometry() const;
    void SetVisible(bool pVisible);
    bool IsVisible() const;
    void BringToFront();
    void SetPoppedOut(bool pPoppedOut);  // sadece Standalone (UI'daki "Pop out" dugmesi)
    bool IsPoppedOut() const;

    void SetUiVisible(bool pVisible);     // ust serit, paneller, palet, arama, durum seridi, tooltip kutusu
    bool IsUiVisible() const;
    void SetMouseInput(bool pEnabled);    // surukle / tekerlek / tooltip / cizim tiklamasi (UI'dan bagimsiz)
    bool IsMouseInputEnabled() const;

    // --- Gorunum ---
    void SetCenter(double pLat, double pLon);
    LatLon GetCenter() const;
    void SetZoom(double pZoom);          // MinZoom..MaxZoom'a kirpilir (3..16.5)
    double GetZoom() const;
    static double MinZoom();
    static double MaxZoom();
    void ZoomIn();                       // bir kademe (varsayilan 0.5 = bir tekerlek tiki)
    void ZoomOut();
    void SetZoomStep(double pStep);
    void PanLeft();
    void PanRight();
    void PanUp();
    void PanDown();
    void SetPanStep(double pFractionOfView);  // varsayilan 0.25
    void PanBy(double pDeltaX, double pDeltaY);  // boxel; harita bu kadar kayar (surukleme ile ayni yon)
    void FitBounds(const GeoBounds& pBounds);
    GeoBounds GetVisibleBounds() const;
    double GetMetersPerBoxel() const;            // merkez enleminde olcek
    void SetFollowAircraft(bool pFollow);        // elle kaydirma / GoToAirport takibi kapatir (bugunku gibi)
    bool IsFollowingAircraft() const;
    void CenterOnAircraft();
    bool GoToAirport(const std::string& pIdent);  // sadece harita gider (ucak degil)
    std::vector<AirportMatch> FindAirports(const std::string& pQuery, int pMaxResults) const;
    LatLon ScreenToGeo(int pX, int pY) const;     // global boxel -> enlem/boylam
    bool GeoToScreen(double pLat, double pLon, int* pX, int* pY) const;  // pencere disindaysa false

    // --- Mod ---
    void SetMode(MapMode pMode);
    MapMode GetMode() const;
    void NextMode();

    // --- Katmanlar (kurallar katman paneliyle birebir ayni) ---
    void SetLayerVisible(MapLayer pLayer, bool pVisible);
    bool IsLayerVisible(MapLayer pLayer) const;   // kendi kutusu; ust oge kapaliysa cizilmez
    void ToggleLayer(MapLayer pLayer);             // panel kutusuna tiklamanin aynisi
    void SetLayersVisible(std::initializer_list<MapLayer> pLayers, bool pVisible);
    void ShowOnlyLayers(std::initializer_list<MapLayer> pLayers);
    void ResetLayersToDefaults();
    LayerState GetLayerState(MapLayer pLayer) const;

    // --- Cizimler ---
    void SetDrawTool(DrawTool pTool);
    DrawTool GetDrawTool() const;
    bool AddDrawingPoint(double pLat, double pLon);  // Pan aracinda false
    bool FinishDrawing();
    bool UndoDrawing();
    void ClearDrawings();
    bool HasUnfinishedDrawing() const;
    std::vector<Shape> GetShapes() const;
    bool RemoveShape(int pId);
    std::string ExportDrawings() const;              // bugunku map_drawings.txt metni
    bool ImportDrawings(const std::string& pText);

    // --- Ucak, iz, rota ---
    void SetAircraftSource(DataSource pSource);
    DataSource GetAircraftSource() const;
    void SetAircraft(const AircraftState& pState);   // Host iken her karede
    AircraftState GetAircraft() const;
    void ClearTrack();
    void SetRouteSource(DataSource pSource);
    DataSource GetRouteSource() const;
    void SetRoute(const std::vector<RouteWaypoint>& pWaypoints, int pActiveIndex);

    // --- Sorgu (tooltip karsiligi; son cizilen kareye gore) ---
    std::vector<MapObject> QueryAt(double pLat, double pLon) const;
    std::vector<MapObject> QueryAtScreen(int pX, int pY) const;

    // --- Ayarlar (disa / ice aktarma; dosyayi host yazar) ---
    std::string ExportSettings() const;  // [view] mode/center/zoom/follow, [layers.<mod>] x5, [tools]
    bool ImportSettings(const std::string& pText);
    TerrainSettings GetTerrainSettings() const;
    void SetTerrainSettings(const TerrainSettings& pSettings);
    std::string ExportTerrainSettings() const;  // terrain_colors.ini metni (yorumlariyla)
    bool ImportTerrainSettings(const std::string& pText);
    // Ne zaman kaydetmeli: degisince artan sayaclar (host her karede bakip degistiyse yazar)
    uint64_t GetSettingsRevision() const;
    uint64_t GetDrawingsRevision() const;

    // --- Zemin on onbellegi ve teshis ---
    bool PrecacheVisibleArea();
    bool PrecacheArea(const GeoBounds& pArea);
    void CancelPrecache();
    void SetDiagnostics(bool pOn);
    bool IsDiagnosticsOn() const;

private:
    Map();
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

}  // namespace PrimeMap
