#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "XPLMDataAccess.h"
#include "XPLMDisplay.h"

#include "Color.h"
#include "Geo.h"
#include "LabelPlacer.h"
#include "MapImageCache.h"
#include "MapView.h"
#include "PrimeMap.h"
#include "ScreenMath.h"
#include "gfx/Canvas.h"
#include "MapTheme.h"
#include "Settings.h"
#include "ui/AirportSearchPanel.h"
#include "ui/AtlasTextRenderer.h"
#include "ui/LayerPanel.h"
#include "ui/ModePanel.h"
#include "ui/StatusBar.h"
#include "ui/ToolPalette.h"
#include "ui/Toolbar.h"

class AircraftData;
class AirportGeometryStore;
class Drawings;
class LayerManager;

struct drawContext;

// Pencereyi acarken verilenler (API MapConfig'in ic karsiligi)
struct MapWindowOptions
{
    RectI mGeometry;           // global boxel
    bool mEmbedded = true;     // false: Standalone
    bool mVisible = true;
    bool mUiVisible = false;
    bool mMouseInput = false;
    MapMode mMode = MapMode::Scenery;
    bool mImageCache = true;   // [render] cache
    int mMaxRenderFps = 15;    // [render] max_fps
    bool mXPlaneFont = false;  // [text] renderer = xplane
};

class XMapWindow
{
public:
    // pAirports: "Go to airport" aramasi
    XMapWindow(LayerManager& pLayers, AircraftData& pAircraft, Drawings& pDrawings,
              const AirportGeometryStore& pAirports);
    ~XMapWindow();

    XMapWindow(const XMapWindow&) = delete;
    XMapWindow& operator=(const XMapWindow&) = delete;

    // Pencereyi nesnedeki ayarlarla (yer, stil, gorunurluk ...) olusturur
    void create();
    // Eski yol: once ayarlari uygular, sonra olusturur
    void create(const MapWindowOptions& pOptions);
    void applyOptions(const MapWindowOptions& pOptions);
    void destroy();
    bool isCreated() const;

    void setEmbedded(bool pEmbedded);  // sadece pencere acik DEGILKEN (stil acik pencerede degismez); degistiyse true doner
    bool isEmbedded() const;
    bool wantsVisible() const;         // pencere acilinca gorunur olacak mi (setVisible ile verilen)
    void setTitle(const std::string& pTitle);  // Standalone pencerenin baslik yazisi
    const std::string& title() const;
    void setImageCacheEnabled(bool pEnabled);  // [render] cache
    bool isImageCacheEnabled() const;
    void setMaxRenderFps(int pFps);            // [render] max_fps
    int maxRenderFps() const;
    void setXPlaneFont(bool pXPlaneFont);      // [text] renderer = xplane; acik pencerede de degisir
    bool usesXPlaneFont() const;
    void setZoomStep(double pStep);            // tekerlek tiki ve zoomIn / zoomOut adimi
    double zoomStep() const;
    void setPanStep(double pFraction);         // panLeft ... adimi (gorunen alanin orani)
    double panStep() const;
    void setZoomLimits(double pMinZoom, double pMaxZoom);
    Settings& settingsStore();                 // bellekteki ayarlar (XMapInstance [window] / [sources] ekler)
    void invalidateImage();                    // harita resmi onbellegi bir sonraki karede yeniden cizilsin

    bool isVisible() const;
    void setVisible(bool pVisible);
    void toggleVisible();
    void toggleControls();
    void setDiagnostics(bool pOn);

    // Harita modu: katman gorunurlugu mod basina saklanir (gecerli mod kaydedilip
    // yenisininki yuklenir)
    void setMapMode(MapMode pMode);
    void nextMapMode();
    MapMode getMapMode() const;

    // Haritada gorunen alan (bolge onbellegi komutu icin)
    GeoBounds visibleBounds() const;

    // Alt seritte gosterilecek kisa durum yazisi (bos = yok).
    // Ornek: "Loading nav data: 120000"
    void setStatusMessage(const std::string& pMessage);

    // Veri yukleniyor mu (zemin, havalimani ayrintisi, nav ...): harita resmi bu surece
    // hiz sinirinda (max_fps) surekli yeniden cizilir, bitince bir kez daha
    void setDataBusy(bool pBusy);

    // Flight loop'tan her karede: pencerenin X dugmesiyle kapatildigini fark eder
    void update(float pNow);

    // Gorunum, palet ve katman ayarlarini bellekteki ayara yazar, SettingsRevision'i artirir
    void saveSettings();

    XMapView& view();
    const XMapView& view() const;
    void setGeometry(const RectI& pGeometry);
    RectI getGeometry() const;
    void bringToFront();
    void setPoppedOut(bool pPoppedOut);
    bool isPoppedOut() const;
    void setUiVisible(bool pVisible);
    bool isUiVisible() const;
    void setMouseInput(bool pEnabled);
    bool isMouseInputEnabled() const;
    void setFollowAircraft(bool pFollow);
    bool isFollowingAircraft() const;
    void centerOnAircraft();
    void goToAirport(int pAirport);  // haritayi havalimanina tasir (ucaga dokunmaz), takibi kapatir
    bool isDiagnosticsOn() const;
    void resetLayers();              // bu modun varsayilanlari ("Defaults" dugmesi)
    void drawingsChanged();          // cizim degisti: DrawingsRevision artar
    std::string exportSettings();    // [view] [tools] [layers.*]
    void importSettings(const std::string& pText);
    // Son cizilen kareye gore (pX, pY) altindaki seyler: katman kimligi + metin (en ustteki once)
    std::vector<std::pair<std::string, std::string>> describeAt(int pX, int pY);
    std::string performanceText() const;
    uint64_t settingsRevision() const;
    uint64_t drawingsRevision() const;

    bool setLayerColor(prime::MapLayer pLayer, const Rgba& pColor);  // yazilamiyorsa false
    Rgba layerColor(prime::MapLayer pLayer) const;  // yazma yoksa mR < 0 doner
    void clearLayerColor(prime::MapLayer pLayer);
    void clearLayerColors();
    void setLayerColors(const std::map<prime::MapLayer, Rgba>& pColors);  // ayar dosyasindan toplu yukleme
    const std::map<prime::MapLayer, Rgba>& layerColors() const;

    // API onRender: bir kare cizer (XPLM cizim callback'iyle ayni yol)
    void render();

private:
    void applyLayerColors();  // mLayerColors -> mTheme (mod degisince yeniden uygulanir)

    // XPLM callback'leri: refcon bu nesnenin adresi
    static void drawCallback(XPLMWindowID pWindow, void* pRefcon);
    static int clickCallback(XPLMWindowID pWindow, int pX, int pY, XPLMMouseStatus pStatus, void* pRefcon);
    static int rightClickCallback(XPLMWindowID pWindow, int pX, int pY, XPLMMouseStatus pStatus,
                                  void* pRefcon);
    static void keyCallback(XPLMWindowID pWindow, char pKey, XPLMKeyFlags pFlags, char pVirtualKey,
                            void* pRefcon, int pLosingFocus);
    static XPLMCursorStatus cursorCallback(XPLMWindowID pWindow, int pX, int pY, void* pRefcon);
    static int wheelCallback(XPLMWindowID pWindow, int pX, int pY, int pWheel, int pClicks, void* pRefcon);

    void onDraw();
    bool prepareCanvas();  // openGL hazir mi (ilk karede yukler); degilse hatayi pencereye yazar
    void createTextRenderer();  // ilk karede: kendi yazi atlasimiz ([text] renderer = atlas | xplane)
    void drawTooltip(const drawContext& pContext, const std::string& pText);  // imlecin yaninda kucuk aciklama
    int onClick(int pX, int pY, XPLMMouseStatus pStatus);
    int onRightClick(int pX, int pY, XPLMMouseStatus pStatus);
    int onWheel(int pX, int pY, int pWheel, int pClicks);
    void rememberMouse(int pX, int pY);
    void onKey(char pKey, XPLMKeyFlags pFlags, char pVirtualKey, bool pLosingFocus);

    void drawControls(int pMouseX, int pMouseY, bool pMouseInside);
    bool handleControlClick(int pX, int pY);  // kontrollere tiklandiysa true
    void handleMapClick(int pX, int pY);      // haritaya (surukleme olmadan) tiklandi
    void performToolbarAction(ToolbarAction pAction);
    void applyLayerPanelHit(const LayerPanel::Hit& pHit);
    RectI paletteBounds() const;  // cizim paletinin kalabilecegi alan (seritlerin arasi)
    void togglePopOut();
    // "Go to airport": paneli acar/kapatir; panel acikken klavye odagi bizde (tuslar X-Plane'e gitmez)
    void setSearchOpen(bool pOpen);
    void setKeyboardFocus(bool pFocus);
    bool hasKeyboardFocus() const;
    ToolbarState currentToolbarState() const;
    RectI showControlsButtonRect() const;
    bool mouseInside(int* pX, int* pY) const;
    bool isOverControls(int pX, int pY) const;
    RectD clearArea() const;
    std::string hintText() const;
    void logDiagnostics(float pNow, double pPixelsPerBoxel, const RectD& pScissor);
    uint64_t sceneSignature() const;  // degisince harita resmi hemen yeniden cizilir (mod, katman, cizim)

    void applySettings();  // bellekteki [view] [tools] [layers.*] ... ayarlarini uygular
    void saveDrawings();
    void useOwnText();     // Ui yazi cizicisi olarak bu pencereninkini sec (her cizim / tiklama basinda)

    LayerManager& mLayers;
    AircraftData& mAircraft;
    Settings mSettings;  // bellekte (dosya yok): mod basina katman durumu, gorunum, palet
    Drawings& mDrawings;
    uint64_t mSettingsRevision = 0;
    uint64_t mDrawingsRevision = 0;
    bool mEmbedded = true;
    bool mMouseInput = false;
    bool mXPlaneFont = false;
    bool mTextPending = false;   // yazi cizici degisti: bir sonraki cizimde yeniden kur
    bool mWantVisible = true;    // pencere acilinca gorunsun mu
    std::string mTitle = "XMap";
    int mMaxRenderFps = 15;
    double mZoomStep = 0.5;      // bir tekerlek tiki
    double mPanStep = 0.25;      // gorunen alanin orani
    bool mPassClick = false;  // fare kapali: bu basma alttaki pencereye gecirildi
    double mLastPixelsPerBoxel = 1.0;

    XPLMWindowID mWindow = nullptr;
    XMapView mView;
    Gfx::Canvas mCanvas;               // tum seklin cizildigi kapi (modern openGL)
    bool mCanvasChecked = false;
    std::unique_ptr<AtlasTextRenderer> mText;  // nullptr: X-Plane'in yazisi (yedek)
    bool mContextWarned = false;  // pop-out baglaminda shader yok uyarisi bir kez loglandi
    Toolbar mToolbar;
    LayerPanel mLayerPanel;
    ModePanel mModePanel;
    const AirportGeometryStore& mAirports;
    AirportSearchPanel mSearchPanel;
    ToolPalette mToolPalette;          // yuzen cizim araclari paneli
    MapMode mMapMode = MapMode::Scenery;
    MapTheme mTheme = themeFor(MapMode::Scenery);  // etkin tema (katman renk yazmalari uygulanmis)
    std::map<prime::MapLayer, Rgba> mLayerColors;  // API setColor yazmalari (ayar dosyasina da yazilir)
    StatusBar mStatusBar;
    LabelPlacer mLabelPlacer;          // tum katmanlarin ortak etiket seyreltmesi
    XMapImageCache mImageCache;         // 0.18: harita resmi (FBO); [render] cache = 0 ile kapatilir
    bool mUseImageCache = true;
    bool mImageCacheWarned = false;
    uint64_t mLastSignature = 0;
    bool mDataBusy = false;
    bool mWasBusy = false;
    std::string mStatusMessage;

    // Teshis: X-Plane'in kare suresi ve bizim cizim suremiz
    XPLMDataRef mFramePeriodRef = nullptr;
    double mDrawMs = 0.0;              // yumusatilmis (ortalama) cizim suresi

    RectI mArea;                       // son cizimdeki harita alani (global boxel)
    RectI mFloatingGeometry;           // X-Plane icindeyken son yer/boyut (kayit icin)
    bool mHasFloatingGeometry = false;
    bool mWasVisible = false;

    bool mFollowAircraft = true;
    bool mShowControls = true;
    bool mDiagnostics = false;
    float mLastDiagnosticsTime = -10.0f;

    // Haritaya basili tutma: bir esikten fazla hareket ederse kaydirma,
    // etmezse birakinca tiklama sayilir
    bool mPressOnMap = false;
    bool mPressMoved = false;
    bool mPaletteDrag = false;         // cizim paleti basligindan tasiniyor
    int mPressX = 0;
    int mPressY = 0;
    int mDragLastX = 0;
    int mDragLastY = 0;

    // Imlecin son bilinen yeri. Pencere disari alinmissa (pop-out)
    // XPLMGetMouseLocationGlobal'a guvenmiyoruz; cursor callback'in
    // verdigi konumu ve zamanini kullaniyoruz.
    int mMouseX = 0;
    int mMouseY = 0;
    float mMouseTime = -10.0f;
};
