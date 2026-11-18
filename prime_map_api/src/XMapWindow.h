// XMap pencerisinin(host tarafinin) ust duzey sahibi: XPLM penceresi (LEGACY) ya da
// dokuya cizilen Texture modu; toolbar, paneller, ayarlar ve fare/klavye girdileri
// burada birlesir. Cizim isini Gfx::Canvas'a, kamera isini XMapView'e devreder;
// X-Plane callback'leri (draw, click, key) refcon uzerinden bu sinifa yonlenir.
#ifndef PRIME_MAP_SRC_XMAPWINDOW_H
#define PRIME_MAP_SRC_XMAPWINDOW_H

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "XPLMDataAccess.h"
#include "XPLMDisplay.h"

#include "Color.h"
#include "Geo.h"
#include "LabelPlacer.h"
#include "XMapImageCache.h"
#include "XMapView.h"
#include "api/XMapTypes.h"
#include "ScreenMath.h"
#include "gfx/Canvas.h"
#include "gfx/RenderTarget.h"
#include "XMapTheme.h"
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

class XMapWindow
{
public:
    // pAirports: "Go to airport" aramasi
    XMapWindow(LayerManager& pLayers, AircraftData& pAircraft, Drawings& pDrawings,
              const AirportGeometryStore& pAirports);
    ~XMapWindow();

    XMapWindow(const XMapWindow&) = delete;
    XMapWindow& operator=(const XMapWindow&) = delete;

    // Stil, yer, gorunurluk ... nesne ayarlariyla acar: Texture'da doku hazirligi,
    // LEGACY stillerde XPLM penceresi
    void create();
    void destroy();
    bool isCreated() const;

    // LEGACY: Embedded (cercevesiz, host yerlestirir) / Standalone; Texture'da da true.
    // Sadece kapaliyken degisir.
    void setEmbedded(bool pEmbedded);
    bool isEmbedded() const;
    bool wantsVisible() const;
    void setTitle(const std::string& pTitle);
    const std::string& title() const;
    void setImageCacheEnabled(bool pEnabled);  // [render] cache
    bool isImageCacheEnabled() const;
    void setMaxRenderFps(int pFps);            // [render] max_fps
    int maxRenderFps() const;
    void setXPlaneFont(bool pXPlaneFont);      // [text] renderer = xplane
    bool usesXPlaneFont() const;
    void setZoomStep(double pStep);
    double zoomStep() const;
    void setPanStep(double pFraction);
    double panStep() const;
    void setZoomLimits(double pMinZoom, double pMaxZoom);
    Settings& settingsStore();
    void invalidateImage();  // harita resmi bir sonraki karede yeniden cizilsin

    bool isVisible() const;
    void setVisible(bool pVisible);
    void toggleVisible();
    void toggleControls();
    void setDiagnostics(bool pOn);

    // Katman gorunurlugu mod basina saklanir (gecerli mod kaydedilip yenisi yuklenir)
    void setMapMode(XMapMode pMode);
    void nextMapMode();
    XMapMode getMapMode() const;

    GeoBounds visibleBounds() const;

    void setStatusMessage(const std::string& pMessage);

    // Veri yukleniyor mu: harita bu surece max_fps hizinda yeniden cizilir
    void setDataBusy(bool pBusy);

    // Flight loop'tan her karede: X dugmesiyle kapatilmanin fark edilmesi
    void update(float pNow);

    // Gorunum, palet ve katman ayarlarini bellekteki ayara yazar, SettingsRevision artar
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
    void goToAirport(int pAirport);  // haritayi tasir (ucaga dokunmaz), takibi kapatir
    bool isDiagnosticsOn() const;
    void resetLayers();      // bu modun varsayilanlari
    void drawingsChanged();  // DrawingsRevision artar
    std::string exportSettings();  // [view] [tools] [layers.*]
    void importSettings(const std::string& pText);
    std::vector<std::pair<std::string, std::string>> describeAt(int pX, int pY);
    std::string performanceText() const;
    uint64_t settingsRevision() const;
    uint64_t drawingsRevision() const;

    bool setLayerColor(prime::XMapLayer pLayer, const Rgba& pColor);  // yazilamiyorsa false
    Rgba layerColor(prime::XMapLayer pLayer) const;                   // yazma yoksa mR < 0
    void clearLayerColor(prime::XMapLayer pLayer);
    void clearLayerColors();
    void setLayerColors(const std::map<prime::XMapLayer, Rgba>& pColors);
    const std::map<prime::XMapLayer, Rgba>& layerColors() const;

    void render();  // bir kare cizer (XPLM cizim callback'iyle ayni yol; Texture: renderTexture)

    // --- Texture modu (WindowStyle::Texture): XPLM penceresi yok, harita mTextureTarget'a cizilir ---
    void setTextureMode(bool pTexture);  // sadece kapaliyken
    bool isTextureMode() const;
    // pWidth x pHeight boxel, orijin sol-alt; pPixelsPerBoxel 0 = host'un UI olcegi
    void setTextureSize(int pWidth, int pHeight, double pPixelsPerBoxel);
    int textureWidth() const;
    int textureHeight() const;
    // Host'un XPLM cizim callback'i icinde: gerekiyorsa dokuyu yeniden cizer. false: basilacak doku yok
    bool renderTexture();
    unsigned int textureId() const;
    // Host'tan gelen girdi (doku koordinati, boxel); XPLM callback'leriyle ayni anlamda
    int injectMouse(int pX, int pY, XPLMMouseStatus pStatus, bool pRight);
    void injectMouseMove(int pX, int pY);
    int injectWheel(int pX, int pY, int pClicks);
    bool injectKey(char pKey, XPLMKeyFlags pFlags, char pVirtualKey);  // klavye bizde degilse false
    bool wantsKeyboard() const;  // arama paneli yaziyor: host klavye odagini almali

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
    void prepareFrame();      // gorunum alani, ucak takibi, serit yerlesimi (GL gerekmez)
    bool beginCanvasFrame();  // Canvas bu baglamda kullanilabilir mi
    // pBoxelToPixel: UI olcegi (yoksa onbellek ve olcek yok); Texture'da harita resmi
    // onbellegi kapalidir (zaten dokuya ciziliyor).
    void drawFrame(float pNow, const std::optional<Affine2D>& pBoxelToPixel, bool pUseImageCache,
                   const RectD& pScissorPixels);
    RectI textureArea() const;
    bool prepareCanvas();      // openGL hazir mi; degilse hatayi pencereye yazar
    void createTextRenderer(); // ilk karede: kendi yazi atlasimiz
    void drawTooltip(const drawContext& pContext, const std::string& pText);
    int onClick(int pX, int pY, XPLMMouseStatus pStatus);
    int onRightClick(int pX, int pY, XPLMMouseStatus pStatus);
    int onWheel(int pX, int pY, int pWheel, int pClicks);
    void rememberMouse(int pX, int pY);
    void onKey(char pKey, XPLMKeyFlags pFlags, char pVirtualKey, bool pLosingFocus);

    void drawControls(int pMouseX, int pMouseY, bool pMouseInside);
    bool handleControlClick(int pX, int pY);
    void handleMapClick(int pX, int pY);
    void performToolbarAction(ToolbarAction pAction);
    void applyLayerPanelHit(const LayerPanel::Hit& pHit);
    RectI paletteBounds() const;
    void togglePopOut();
    // Panel acikken klavye odagi bizde (tuslar X-Plane'e gitmez)
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
    uint64_t sceneSignature() const;  // degisince harita hemen yeniden cizilir

    void applySettings();
    void saveDrawings();
    void useOwnText();  // yazi cizicisi olarak bu pencereninkini sec

    LayerManager& mLayers;
    AircraftData& mAircraft;
    Settings mSettings;  // bellekte: mod basina katman durumu, gorunum, palet
    Drawings& mDrawings;
    uint64_t mSettingsRevision = 0;
    uint64_t mDrawingsRevision = 0;
    bool mEmbedded = true;       // false: Standalone (LEGACY); Texture'da true
    bool mMouseInput = false;
    bool mXPlaneFont = false;
    bool mTextPending = false;   // yazi cizici degisti: sonraki cizimde yeniden kur
    bool mWantVisible = true;    // pencere acilinca gorunsun mu
    std::string mTitle = "XMap";
    int mMaxRenderFps = 15;
    double mZoomStep = 0.5;
    double mPanStep = 0.25;
    bool mPassClick = false;     // fare kapali: basma alttaki pencereye gecirildi
    double mLastPixelsPerBoxel = 1.0;

    XPLMWindowID mWindow = nullptr;
    XMapView mView;
    Gfx::Canvas mCanvas;  // seklin cizildigi kapi (modern openGL)
    bool mCanvasChecked = false;
    std::unique_ptr<AtlasTextRenderer> mText;  // nullptr: X-Plane'in yazisi
    bool mContextWarned = false;
    Toolbar mToolbar;
    LayerPanel mLayerPanel;
    ModePanel mModePanel;
    const AirportGeometryStore& mAirports;
    AirportSearchPanel mSearchPanel;
    ToolPalette mToolPalette;
    XMapMode mMapMode = XMapMode::Dark;
    XMapTheme mTheme = themeFor(XMapMode::Dark);  // etkin tema (renk yazmalari uygulanmis)
    std::map<prime::XMapLayer, Rgba> mLayerColors;   // API setColor yazmalari
    StatusBar mStatusBar;
    LabelPlacer mLabelPlacer;  // tum katmanlarin ortak etiket seyreltmesi
    XMapImageCache mImageCache;  // harita resmi FBO'su; [render] cache = 0 ile kapali
    bool mUseImageCache = true;
    bool mImageCacheWarned = false;
    uint64_t mLastSignature = 0;
    bool mDataBusy = false;
    bool mWasBusy = false;
    std::string mStatusMessage;

    // Teshis: X-Plane kare suresi ve bizim cizim suremiz
    XPLMDataRef mFramePeriodRef = nullptr;
    double mDrawMs = 0.0;  // yumusatilmis

    RectI mArea;              // son cizimdeki harita alani (global boxel)
    RectI mFloatingGeometry;  // X-Plane icindeyken son yer/boyut
    bool mHasFloatingGeometry = false;
    bool mWasVisible = false;

    bool mFollowAircraft = true;
    bool mShowControls = true;
    bool mDiagnostics = false;
    float mLastDiagnosticsTime = -10.0f;

    // Basili tutma: esikten fazla hareket kaydirma, etmezse birakinca tiklama sayilir
    bool mPressOnMap = false;
    bool mPressMoved = false;
    bool mPaletteDrag = false;
    int mPressX = 0;
    int mPressY = 0;
    int mDragLastX = 0;
    int mDragLastY = 0;

    // Texture modu: host'a verilen doku ve yeniden cizim karari
    bool mTextureMode = false;
    bool mTextureOpen = false;            // create / destroy
    int mTextureWidth = 400;              // boxel
    int mTextureHeight = 300;
    double mTexturePixelsPerBoxel = 0.0;  // 0: host'un UI olcegi
    Gfx::renderTarget mTextureTarget;
    bool mTextureDirty = true;            // hiz sinirini beklemeden ciz
    bool mTextureKeyboard = false;        // arama paneli klavye istiyor
    bool mTextureFontWarned = false;
    float mLastTextureRender = -100.0f;
    double mTextureCenterX = 0.0;         // son cizimin gorunumu; degisince hemen ciz
    double mTextureCenterY = 0.0;
    double mTextureZoom = -1.0;

    // Imlecin son bilinen yeri ve zamani: pop-out baglaminda XPLMGetMouseLocationGlobal
    // guvenilir degil, cursor callback'in konumu kullanilir.
    int mMouseX = 0;
    int mMouseY = 0;
    float mMouseTime = -10.0f;
};
#endif  // PRIME_MAP_SRC_XMAPWINDOW_H
