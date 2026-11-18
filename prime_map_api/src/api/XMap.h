// prime::XMap: TEK bir haritanin her seyi. Pencere, katmanlar, cizimler, ucak/rota kaynaklari
// ve ayar dosyalari bu nesnenin icinde; ortak veri (nav, havalimanlari, zemin iscisi, onbellek)
// XMapData'dadir, katmanlar oraya referansla bakar. Nesneyi yalnizca XMapManager olusturur
// (addXMap) ve yok eder (removeXMap); butun metotlar X-Plane ana thread'inden cagrilmalidir.
// LIF-08: yonetici shutdown() ettikten sonra harita isaretcisi GECERLIDIR ama metotlar guvenli
// no-op'tur (bir kez Log uyarisı, false / bos / varsayilan deger). Buna karsilik removeXMap ya
// da destroyXMapManager sonrasi isaretci gecersizdir (nesne silinir); bu durum korunamaz, host
// isaretcisini kullanmamalidir.
#ifndef PRIME_MAP_SRC_API_XMAP_H
#define PRIME_MAP_SRC_API_XMAP_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Drawings.h"
#include "LayerManager.h"
#include "XMapWindow.h"
#include "ScreenMath.h"
#include "XMapTypes.h"
#include "data/AircraftData.h"
#include "data/TerrainClasses.h"

class FmsLayer;
class Settings;
class TerrainLayer;
class TrackLayer;
class XMapData;

namespace prime
{

class XMapManager;

// C4251 (dll arayuzunde std uyeleri): zararsiz; /wd4251 ile tum hedeflerde kapatilir (pragma yok).

class PRIME_MAP_EXPORT XMap
{
public:
    ~XMap();

    bool initialize();            // pencereyi acar, dosyalari okur; veri yuklemesi arka planda surer
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
    // Stil: Texture onerilen ve hedef yol. Embedded / Standalone LEGACY (WindowStyle); setTitle, setGeometry,
    // bringToFront, setPoppedOut yalnizca bu LEGACY pencereli stillerde anlamlidir.
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
    void setMode(XMapMode pMode);
    XMapMode getMode() const;
    void nextMode();

    // --- Katmanlar ---
    void setIsLayerVisible(XMapLayer pLayer, bool pVisible);
    bool isLayerVisible(XMapLayer pLayer) const;
    void toggleLayer(XMapLayer pLayer);
    void setLayersVisible(std::initializer_list<XMapLayer> pLayers, bool pVisible);
    void showOnlyLayers(std::initializer_list<XMapLayer> pLayers);
    void resetLayersToDefaults();
    LayerState getLayerState(XMapLayer pLayer) const;
    bool setLayerZoomRange(XMapLayer pLayer, double pMinZoom, double pMaxZoom);
    void getLayerZoomRange(XMapLayer pLayer, double* pMinZoom, double* pMaxZoom) const;
    void clearLayerZoomRange(XMapLayer pLayer);
    void clearAllLayerZoomRanges();
    bool setColor(XMapLayer pLayer, const Rgb& pColor);  // katman cizim rengi (dosyada layer_colors)
    Rgb getColor(XMapLayer pLayer) const;                 // override yoksa mR < 0 doner
    void clearColor(XMapLayer pLayer);
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
    std::vector<XMapObject> queryAt(double pLat, double pLon) const;
    std::vector<XMapObject> queryAtScreen(int pX, int pY) const;

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

    // --- Doku (WindowStyle::Texture) ---
    // Penceresiz openGL dokusu; host kendi ciziminde getTextureId()'yi basar (v = 0 alt kenar).
    // Koordinatlar boxel, orijin SOL-ALT, y yukari; pPixelsPerUnit 0 = host UI olceginden otomatik.
    bool setTextureSize(int pWidth, int pHeight, double pPixelsPerUnit = 0.0);
    int getTextureWidth() const;   // boxel
    int getTextureHeight() const;
    bool renderTexture();          // host'un XPLM cizim callback'i icinde; false: dokuyu basma
    unsigned int getTextureId() const;  // GL_TEXTURE_2D adi (XPLMBindTexture2d); yoksa 0
    // Girdi: host kendi pencere koordinatini doku koordinatina cevirip iletir. Tuketildiyse true.
    bool injectMouse(int pX, int pY, MouseAction pAction, MouseButton pButton = MouseButton::Left);
    void injectMouseMove(int pX, int pY);  // imlec dokunun ustundeyken her karede (tooltip, vurgu)
    bool injectWheel(int pX, int pY, int pClicks);
    bool injectKey(char pKey, int pFlags, char pVirtualKey);  // XPLM key callback degerleri aynen
    bool wantsKeyboard() const;  // true: host klavye odagini alip tuslari injectKey ile iletmeli

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
    friend class XMapManager;  // nesneyi olusturur / yok eder; mXMaps listesini yonetir
    friend class ::XMapData;   // flight loop her karede update() cagirir

    XMap(XMapData& pData, int pId);
    XMap(const XMap&) = delete;
    XMap& operator=(const XMap&) = delete;

    // --- Motor: katmanlar ve dosyalar ---
    void createLayers();
    void loadFiles();                 // initialize: terrain_colors.json, map_settings.json, map_drawings.txt
    bool writeSettingsFile();
    bool writeDrawingsFile();
    bool writeTerrainFile();
    void applyWindowSection();        // window: Standalone pencerenin yeri / gorunurlugu (dosyadan)
    void applySourcesSection();
    void applyLayerColorsSection(const Settings& pStore);
    void storeExtraSections() const;  // window, sources -> pencerenin bellekteki ayarlarina
    RectI savedGeometry() const;      // kaydedilecek yer (pop-out'ta X-Plane icindeki son yer)
    uint64_t stateSignature() const;  // revizyon sayaclarinin gormedigi degisiklikler (yer, gorunurluk)
    void teardown();                  // shutdown: dosyalari yaz, pencereyi ve motor parcalarini birak
    bool alive(const char* pWho) const;  // LIF-08: shutdown sonrasi cagrilar icin ortak koruma
    void update(float pNow);          // flight loop'tan her karede (XMapData)
    std::string folderName() const;   // ayar klasoru adi: safeFolderName(name), bos ise map_<id>
    static std::string safeFolderName(const std::string& pName);
    bool saveNow();                   // tum dosyalari hemen yazar
    void markChanged();               // API ile yapilan degisiklik: otomatik kaydi tetikler
    void applyBuiltUpPalette();       // H-01: yerlesim rengini zemin paletine tasi (dokular yenilenir)

    XMapData& mData;
    std::string mMapsDirectory;    // kurulumda alindi (shutdown sonrasi getSettingsDirectory icin)
    int mId = 0;
    std::string mName;
    Persistence mPersistence = Persistence::LoadAndSave;

    std::unique_ptr<AircraftData> mAircraft;
    std::unique_ptr<Drawings> mDrawings;
    std::unique_ptr<TerrainPalette> mPalette;  // bu haritanin zemin ayarlari (X-Plane renkleri uygulanmis)
    int mPaletteId = 0;
    std::unique_ptr<LayerManager> mLayers;     // katmanlar yukaridaki verilere bagli
    TerrainLayer* mTerrainLayer = nullptr;     // sahibi mLayers
    FmsLayer* mFmsLayer = nullptr;
    TrackLayer* mTrackLayer = nullptr;
    std::unique_ptr<XMapWindow> mWindow;       // mLayers / mAircraft / mDrawings'e bagli: en son

    // Otomatik kayit
    uint64_t mSavedSettings = 0;
    uint64_t mSavedDrawings = 0;
    uint64_t mSavedState = 0;
    uint64_t mExtraRevision = 0;   // markChanged ile artar
    uint64_t mSavedExtra = 0;
    float mLastSaveTime = -100.0f;
    RectI mFloatingGeometry;       // X-Plane icindeyken son yer (pop-out'ta degismez)
    std::string mStatusText;
    std::atomic<bool>* mDataLoadedFlag = nullptr;  // sahibi bu nesne (mIsDataLoaded); update() yazar

    std::atomic<bool> mIsDataLoaded{false};
    bool mTextureWarned = false;      // renderTexture yanlis stilde: uyari bir kez (her karede cagrilir)
    mutable bool mShutdownWarned = false;  // LIF-08: shutdown sonrasi ilk no-op cagrida bir kez uyari
    XMapManager* mManager = nullptr;  // sahibi; yok olurken kendini listeden dusurur
};

}  // namespace prime

#endif  // PRIME_MAP_SRC_API_XMAP_H
