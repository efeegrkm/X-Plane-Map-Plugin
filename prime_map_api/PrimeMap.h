// PrimeMap.h
// Prime Map API: X-Plane 11 haritasini baska bir X-Plane eklentisinin icine gomer.
// Bu dosya kutuphanenin TEK genel basligidir:
// sadece standart C++ tipleri kullanir; XPLM, OpenGL ya da projenin ic basliklari yok.
//
// Kutuphane SHARED DLL olarak derlenir (prime_map.dll). Tuketici projede:
//   add_subdirectory(<prime_map_api yolu> prime_map_build)
//   target_link_libraries(<hedef> PRIVATE prime_map)
// Derlemeden sonra prime_map.dll, tuketici eklentisinin (win.xpl) yanina kopyalanir.
//
// Yasam dongusu (hepsi X-Plane ana thread'inde):
//
//   XPluginEnable  : auto& tLib = prime::MapLibrary::Get();
//                    tLib.Init(tConfig)            kutuphane baslar (veri arka planda yuklenir)
//                    tMap = tLib.CreateMap()       FABRIKA: harita nesnesi (pencere henuz YOK)
//                    tMap->SetName("ios_left")     istege bagli: ayar klasorunun adi
//                    tMap->SetGeometry(...)        yer, stil, mod, katmanlar, zoom sinirlari ... :
//                    tMap->SetMode(...)            butun ayarlar pencere acilmadan verilebilir
//                    tMap->Open()                  pencere bu ayarlarla acilir
//   ...            : tMap->ZoomIn(), tMap->SetLayerVisible(...) ...   acikken de ayni fonksiyonlar
//                    tMap2 = tLib.CreateMap(); ... tMap2->Open()      ikinci, ucuncu ... harita
//   XPluginDisable : tMap.reset()                  ayarlar yazilir, pencere kapanir
//                    tLib.Shutdown()               arka plan isleri durur
//
// Her harita BAGIMSIZDIR: kendi gorunumu, modu, katmanlari, cizimleri, ucak / rota kaynagi,
// zemin ayarlari ve ayar dosyalari vardir. ORTAK olan sadece veri ve onbellektir (nav,
// havalimanlari, hava yollari, zemin onbellegi): bir haritanin yukledigi karo digerine de yarar.
//
// Dosyalar (hepsi veri klasorunde; varsayilan: host eklentisinin klasoru):
//   prime_map.ini                       merkezi ayarlar (LibrarySettings): onbellek sinirlari ...
//   maps/<ad>/map_settings.ini          haritanin ayarlari   (<ad>: SetName ile verilen ad,
//   maps/<ad>/map_drawings.txt          haritanin cizimleri   verilmediyse "map_<kimlik>")
//   maps/<ad>/terrain_colors.ini        haritanin zemin renkleri / ayarlari
//   cache/terrain/, cache/airports/     ORTAK onbellek (silinebilir)
//
// Harita kendi XPLM penceresinde, kendi flight loop'uyla kendisi cizer. Host sadece
// olusturur, ayarlar, acar ve fonksiyonlari kendi dugmelerine baglar.
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
#include <utility>
#include <vector>

#include "primemapglobal.h"

namespace prime
{

// --- Surum --------------------------------------------------------------------
// Kutuphane surumu: CMake project(VERSION) ile AYNI olmali; CMakeLists bunu
// configure sirasinda denetler (uyusmazsa hata verir). Kirilirse major artar;
// yeni fonksiyon minor, duzeltme patch.
constexpr int kVersionMajor = 0;
constexpr int kVersionMinor = 23;
constexpr int kVersionPatch = 0;
PRIME_MAP_EXPORT std::string VersionString();  // "0.23.0"

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

// Haritanin ayar dosyalari (maps/<ad>/ altinda) nasil kullanilsin
enum class Persistence
{
    Off,          // dosya okunmaz / yazilmaz (host isterse Export... / Import... ile kendi saklar)
    SaveOnly,     // dosya okunmaz: kodla verilen ayarlar her acilista gecerli; degisiklikler yine yazilir
    LoadAndSave,  // varsayilan: Open() dosyalari okur (son oturum geri gelir), degisince yazilir
};

// --- Baslatma ------------------------------------------------------------------
struct InitConfig
{
    // Veri klasoru: prime_map.ini, maps/<ad>/ ve (mCacheDirectory bossa) cache/ bunun altindadir.
    // Bos = host eklentisinin klasoru (.../Resources/plugins/<host>/). Ayni surecteki birden cok
    // host kutuphaneyi paylasir: ILK Init eden host'un klasorleri gecerlidir.
    std::string mDataDirectory;
    std::string mCacheDirectory;            // zemin / havalimani onbellegi; bos = <veri klasoru>/cache/
    std::string mLogPrefix = "[prime-map]";
    // Verilirse log satirlari X-Plane Log.txt yerine buraya gider (onek eklenmis halde, ana thread)
    std::function<void(std::string_view pLine)> mLogCallback;
    // prime_map.ini'nin ILK degerleri: dosyada [cache] terrain_mb / prefetch_tiles yoksa bunlar yazilir.
    // Dosyada varsa DOSYA kazanir (kullanicinin ayari). Calisirken degistirmek: MapLibrary::SetSettings.
    int mTerrainCacheMb = 1024;             // zemin disk onbellegi siniri, TEK harita icin (0 = sinirsiz)
    int mPrefetchTiles = 20;                // onceden yukleme karo sayisi (0 = kapali)
};

// Merkezi ayarlar: veri klasorundeki prime_map.ini (butun haritalar icin ortak).
// Onbellek ortaktir; sinirlari ACIK harita sayisina (n) gore buyur:
//     sinir(n) = taban x (1 + artis% x (n - 1) / 100)      (tavan verilmisse onu gecmez)
// Ornek: mTerrainCacheMb 1024, mCacheGrowthPercentPerMap 25 -> 1 harita 1024 MB, 2 harita 1280 MB,
// 3 harita 1536 MB. Harita kapaninca sinir geri iner, fazlasi (en uzun suredir kullanilmayan
// karolar) silinir.
struct LibrarySettings
{
    // [cache]
    int mTerrainCacheMb = 1024;           // terrain_mb: zemin disk onbellegi, TEK harita icin MB (0 = sinirsiz)
    int mCacheGrowthPercentPerMap = 25;   // growth_percent_per_map: her EK acik harita icin tabanin yuzdesi
    int mTerrainCacheMaxMb = 0;           // max_terrain_mb: buyumus disk sinirinin tavani (0 = tavan yok)
    int mPrefetchTiles = 20;              // prefetch_tiles: ucagin onu + FMS rotasi icin onceden hazirlanan karo (0 = kapali)
    // [memory]
    int mTextureMemoryMb = 512;           // texture_mb: zemin dokulari icin video bellegi, TEK harita icin MB
    int mMemoryGrowthPercentPerMap = 50;  // growth_percent_per_map: her EK acik harita icin (dokular, iscinin
                                          // bellekte tuttugu karolar, yuklu havalimani ayrintilari)
    int mTextureMemoryMaxMb = 2048;       // max_texture_mb: buyumus doku butcesinin tavani (0 = tavan yok)
    // [maps]
    bool mAutoSave = true;                // autosave: harita ayar dosyalari degisince kendiliginden yazilsin
    double mAutoSaveSeconds = 2.0;        // autosave_seconds: ayni haritanin iki yazimi arasi en kisa sure
};

struct Status
{
    bool mNavReady = false;
    bool mAirportsReady = false;
    bool mAirwaysReady = false;
    bool mTerrainIndexReady = false;
    int mTerrainPendingImages = 0;   // "Loading terrain: N images" (butun gorunur haritalarin toplami)
    bool mPrecacheRunning = false;
    int mPrecacheDone = 0;
    int mPrecacheTotal = 0;
    std::string mText;               // birlesik durum metni (ortak yukleme + toplam zemin); bos = yok
    std::string mPerformanceText;    // teshis modundaki ilk haritanin "FPS .. draw .. ms"; yoksa bos
    // 0.23: ortak onbellegin su an gecerli sinirlari (acik harita sayisina gore buyumus)
    int mOpenMaps = 0;               // acik harita sayisi
    int mTerrainCacheLimitMb = 0;    // zemin disk onbellegi siniri (0 = sinirsiz)
    int mTextureLimitMb = 0;         // zemin dokulari icin video bellegi butcesi
};

// --- Kutuphane (baslatma) -------------------------------------------------------
// Kutuphanenin kendisi bir nesnedir (MapLibrary): Init onu baslatir, CreateMap
// uzerinden istenildigi kadar Map nesnesi uretilir. SHARED DLL oldugu icin ayni
// X-Plane surecindeki tum host eklentileri Get() ile TEK ornegi paylasir; ilk
// host'un yapilandirmasi gecerli kalir ve Init/Shutdown referans sayaclidir.
class Map;
struct OpenConfig;

// pimpl uyesi (std::unique_ptr<Impl>) MSVC'de C4251 verir; Impl tamamen DLL icinde
// yasadigi icin uyari her iki tarafta da guvenli sekilde susturulur.
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4251)
#endif
class PRIME_MAP_EXPORT MapLibrary
{
public:
    // Surecin tek kutuphane ornegi. Butun metotlar X-Plane ana thread'inden cagrilmalidir.
    static MapLibrary& Get();
    ~MapLibrary();

    // Kutuphaneyi baslatir; veri yuklemesi arka planda baslar. Kutuphane zaten
    // calisiyorsa (baska host) ek is yapmaz, true doner. Her Init icin kapanista
    // bir Shutdown beklenir.
    bool Init(const InitConfig& pConfig);
    // Referans sayacini dusurur; son host cikarken arka plan isleri durdurulur ve
    // acik kalan harita pencereleri kapatilir (once Map nesnelerini birakmak tercih edilir).
    void Shutdown();
    bool IsInitialized() const;
    Status GetStatus() const;

    // --- Harita fabrikasi (0.23) ---
    // Yeni bir harita NESNESI olusturur; pencere henuz acilmaz. Butun ayarlar nesne uzerinden
    // verilir (SetName, SetGeometry, SetMode, SetLayerVisible ...), sonra Map::Open() cagrilir.
    // Ayni anda istenildigi kadar harita yasayabilir; her biri bagimsizdir. Nesnenin sahibi
    // host'tur (unique_ptr): birakilinca ayarlari yazilir ve penceresi kapanir.
    // Init yapilmadiysa nullptr + log.
    std::unique_ptr<Map> CreateMap();
    // Kisa yol (0.22 ile uyumlu): olusturur, pConfig'i uygular ve HEMEN acar. Bu yolla acilan
    // haritanin ayar dosyalari kapalidir (Persistence::Off): dosyalari host yonetir
    // (ExportSettings / ImportSettings). Sonradan SetPersistence ile acilabilir.
    std::unique_ptr<Map> CreateMap(const OpenConfig& pConfig);

    // Yasayan haritalar (olusturulma sirasiyla; acik ya da degil). Isaretciler, host ilgili
    // unique_ptr'i birakana kadar gecerlidir.
    std::vector<Map*> GetMaps() const;
    Map* FindMap(int pId) const;                    // kimlikle; yoksa nullptr
    Map* FindMap(const std::string& pName) const;   // SetName ile verilen adla; yoksa nullptr
    int GetOpenMapCount() const;

    // --- Merkezi ayarlar (prime_map.ini) ---
    LibrarySettings GetSettings() const;
    // Hemen uygulanir (sinirlar yeniden hesaplanir) ve prime_map.ini'ye yazilir. Gecersiz
    // degerler en yakin gecerli degere cekilir. Init yapilmadiysa false.
    bool SetSettings(const LibrarySettings& pSettings);

    // --- Klasorler (hepsi sonunda ayirici ile; Init yapilmadiysa bos) ---
    std::string GetDataDirectory() const;      // prime_map.ini ve maps/ burada
    std::string GetSettingsFilePath() const;   // .../prime_map.ini
    std::string GetMapsDirectory() const;      // .../maps/
    std::string GetCacheDirectory() const;     // ORTAK onbellek: .../cache/

private:
    MapLibrary();
    MapLibrary(const MapLibrary&) = delete;
    MapLibrary& operator=(const MapLibrary&) = delete;

    struct Impl;
    std::unique_ptr<Impl> mImpl;

    friend class Map;  // Map, yok olurken kendini kutuphanenin harita listesinden dusurur
};
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

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

PRIME_MAP_EXPORT std::optional<MapLayer> ParentOf(MapLayer pLayer);  // en ust oge (grup, Drawings): nullopt
PRIME_MAP_EXPORT std::vector<MapLayer> ChildrenOf(MapLayer pLayer);
PRIME_MAP_EXPORT std::vector<MapLayer> TopLevelLayers();             // gruplar + grupsuz katmanlar, panel sirasiyla
PRIME_MAP_EXPORT LayerKind KindOf(MapLayer pLayer);
PRIME_MAP_EXPORT const char* LayerName(MapLayer pLayer);             // panelde gorunen ad ("Water runways")
PRIME_MAP_EXPORT const char* LayerKey(MapLayer pLayer);              // ayar anahtari ("runways.water"); grup: "group.airports"
PRIME_MAP_EXPORT std::optional<MapLayer> LayerFromKey(std::string_view pKey);
PRIME_MAP_EXPORT bool IsLayerDefaultVisible(MapLayer pLayer, MapMode pMode);

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

// pimpl uyesi (std::unique_ptr<Impl>) MSVC'de C4251 verir; Impl tamamen DLL icinde
// yasadigi icin uyari her iki tarafta da guvenli sekilde susturulur.
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4251)
#endif
class PRIME_MAP_EXPORT Map
{
public:
    // Geriye uyumluluk (0.22): MapLibrary::Get().CreateMap(pConfig) ile ayni (olusturur + hemen acar).
    static std::unique_ptr<Map> Open(const OpenConfig& pConfig);
    ~Map();  // ayarlari yazar (Persistence Off degilse), pencereyi kapatir

    Map(const Map&) = delete;
    Map& operator=(const Map&) = delete;

    // KURAL: asagidaki butun Set... / Get... fonksiyonlari pencere acilmadan ONCE de, acikken de
    // calisir (aksi yazilmadikca). Acilmadan once verilen deger nesnede durur, Open() ile gecerli olur.

    // --- Kimlik, ad, ayar dosyalari (0.23) ---
    int GetId() const;                       // surec icinde tekil, 1'den baslar (olusturulma sirasi)
    // Haritanin adi = ayar klasorunun adi: <veri>/maps/<ad>/. Verilmezse klasor "map_<kimlik>".
    // Harf, rakam, '-', '_', '.' disindaki karakterler klasor adinda '_' olur. Sadece harita
    // KAPALIYKEN degisir; baska bir haritanin kullandigi ad reddedilir (false + log).
    bool SetName(const std::string& pName);
    std::string GetName() const;
    std::string GetSettingsDirectory() const;   // .../maps/<ad>/ (sonunda ayirici)
    void SetPersistence(Persistence pPersistence);  // varsayilan LoadAndSave (CreateMap(OpenConfig): Off)
    Persistence GetPersistence() const;
    bool SaveSettings();                     // dosyalari simdi yazar (Persistence::Off ise false)

    // --- Acma / kapama (0.23) ---
    // Pencereyi nesnedeki ayarlarla acar. Persistence::LoadAndSave ise once ayar dosyalari okunur:
    // dosyadaki deger, Open'dan ONCE kodla verilen degerin yerine gecer (son oturum geri gelir);
    // dosyada olmayan ayar icin kodla verilen deger kalir. Kodun her zaman kazanmasi isteniyorsa:
    // ayari Open'dan SONRA ver ya da Persistence::SaveOnly kullan. Zaten aciksa true.
    bool Open();
    // Ayarlari yazar ve pencereyi kapatir. Nesne ve ayarlari kalir; Open() ile yeniden acilir.
    void Close();
    bool IsOpen() const;

    // --- Pencere ---
    void SetStyle(WindowStyle pStyle);       // sadece harita KAPALIYKEN (acik pencerenin stili degismez)
    WindowStyle GetStyle() const;
    void SetTitle(const std::string& pTitle);  // Standalone pencerenin basligi (varsayilan "Map")
    std::string GetTitle() const;
    void SetGeometry(const Geometry& pGeometry);
    Geometry GetGeometry() const;
    void SetVisible(bool pVisible);          // kapaliyken: Open() bu gorunurlukle acar
    bool IsVisible() const;                  // pencere acik VE gorunur
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
    void SetZoom(double pZoom);          // bu haritanin zoom sinirlarina kirpilir (varsayilan 3..16.5)
    double GetZoom() const;
    static double MinZoom();             // varsayilan alt sinir (3) = mutlak alt sinir
    static double MaxZoom();             // varsayilan ust sinir (16.5)
    static double HardMaxZoom();         // mutlak ust sinir (18)
    // Bu haritanin zoom sinirlari ("en uzak / en yakin bakis"). MinZoom()..HardMaxZoom() disina
    // tasan deger kirpilir; pMinZoom > pMaxZoom ise false + log. Gecerli zoom yeni araliga cekilir.
    bool SetZoomLimits(double pMinZoom, double pMaxZoom);
    double GetMinZoomLimit() const;
    double GetMaxZoomLimit() const;
    void ZoomIn();                       // bir kademe (varsayilan 0.5 = bir tekerlek tiki)
    void ZoomOut();
    void SetZoomStep(double pStep);      // ZoomIn / ZoomOut ve fare tekerlegi adimi, (0, 5]
    double GetZoomStep() const;
    void PanLeft();
    void PanRight();
    void PanUp();
    void PanDown();
    void SetPanStep(double pFractionOfView);  // varsayilan 0.25; (0, 1]
    double GetPanStep() const;
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
    // Zoom araligi (0.23): katman / alt katman sadece pMinZoom..pMaxZoom arasinda cizilir ("su
    // uzakliktan itibaren gorunsun"). Katmanin kendi esikleri gecerli kalir (ornek: pistler 11+);
    // aralik onlari DARALTIR, erkene alamaz. Kutu durumu (IsLayerVisible) degismez. Grup verilirse
    // gruptaki butun katmanlara uygulanir. pMinZoom > pMaxZoom ise false + log.
    bool SetLayerZoomRange(MapLayer pLayer, double pMinZoom, double pMaxZoom);
    // Katmanin araligi (grup icin: ilk katmaninki). Sinir yoksa 0 ve 99 doner.
    void GetLayerZoomRange(MapLayer pLayer, double* pMinZoom, double* pMaxZoom) const;
    void ClearLayerZoomRange(MapLayer pLayer);
    void ClearAllLayerZoomRanges();

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

    // --- Cizim secenekleri (0.23: acikken de degisir) ---
    void SetImageCache(bool pEnabled);    // harita resmi onbellegi (FBO); [render] cache
    bool IsImageCacheEnabled() const;
    void SetMaxRenderFps(int pFps);       // harita resminin en cok yeniden cizilme hizi, 1..240; [render] max_fps
    int GetMaxRenderFps() const;
    void SetXPlaneFont(bool pXPlaneFont); // true: XPLMDrawString (yedek); false: kendi yazi atlasimiz; [text] renderer
    bool IsXPlaneFont() const;

    // --- Ayarlar (metin olarak disa / ice aktarma) ---
    // Kutuphane dosyalari kendisi yonetir (Persistence); bu fonksiyonlar ayni metni host'a da verir
    // (ornek: kendi profil sistemi olan host, bir haritanin ayarini baskasina kopyalamak).
    // [window] [view] [render] [text] [tools] [layers.<mod>] x5 [layer_zoom] [sources]
    std::string ExportSettings() const;
    // Metindeki anahtarlar uygulanir, metinde olmayanlar oldugu gibi kalir. [window] sadece
    // Standalone haritada uygulanir.
    bool ImportSettings(const std::string& pText);
    // Zemin ayarlari BU haritaya aittir (0.23) ve hemen uygulanir: yeni dokular yeni ayarla uretilir,
    // X-Plane'i yeniden baslatmak gerekmez. Ayni ayarli haritalar dokulari paylasir.
    TerrainSettings GetTerrainSettings() const;
    void SetTerrainSettings(const TerrainSettings& pSettings);
    std::string ExportTerrainSettings() const;  // terrain_colors.ini metni (yorumlariyla)
    bool ImportTerrainSettings(const std::string& pText);
    // Degisince artan sayaclar (dosyalarini kendi yoneten host: her karede bakip degistiyse yazar)
    uint64_t GetSettingsRevision() const;
    uint64_t GetDrawingsRevision() const;

    // --- Durum (0.23) ---
    std::string GetStatusText() const;        // bu haritanin alt seridindeki yazi (bos = yok)
    int GetTerrainPendingImages() const;      // bu haritada bekleyen zemin dokusu

    // --- Zemin on onbellegi ve teshis ---
    bool PrecacheVisibleArea();
    bool PrecacheArea(const GeoBounds& pArea);
    void CancelPrecache();
    void SetDiagnostics(bool pOn);
    bool IsDiagnosticsOn() const;

private:
    Map();
    friend class MapLibrary;  // nesneyi yalnizca kutuphane kurar

    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

}  // namespace prime
