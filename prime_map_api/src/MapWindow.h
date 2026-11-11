// MapWindow.h
// Haritanin kendi penceresi (XPLMCreateWindowEx). Pencerenin tamami harita;
// kontroller haritanin ustunde, ince ve yari saydam.
//
// Gorevleri:
//   - pencereyi olusturmak, gostermek/gizlemek, X-Plane disina almak (pop-out)
//   - fare: surukleme kaydirir (her modda), tekerlek imlecin etrafinda zoom;
//     cizim modlarinda tiklama kose ekler, sag tik rota/bolgeyi bitirir
//   - her karede cizim sirasi: zemin rengi -> katmanlar -> kontroller
//   - gorunum ve katman ayarlarini BELLEKTE tutmak (0.20: dosyaya host yazar; API
//     ExportSettings / ImportSettings, degisince SettingsRevision / DrawingsRevision artar)
//
// 0.20 (API): iki pencere stili. Standalone = bugunku pencere (cerceve, pop-out, gizli
// kontrollerde "Controls" dugmesi). Embedded = cercevesiz, yeri sadece SetGeometry ile.
// UI (seritler, paneller) ve fare (surukle / tekerlek / tooltip / cizim) ayri anahtarlar:
// fare kapaliyken haritaya tiklama alttaki pencereye gecirilir (0 dondurulur).
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "XPLMDataAccess.h"
#include "XPLMDisplay.h"

#include "Geo.h"
#include "LabelPlacer.h"
#include "MapImageCache.h"
#include "MapView.h"
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

struct DrawContext;

// Pencereyi acarken verilenler (API OpenConfig'in ic karsiligi)
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

class MapWindow
{
public:
    // pAirports: "Go to airport" aramasi
    MapWindow(LayerManager& pLayers, AircraftData& pAircraft, Drawings& pDrawings,
              const AirportGeometryStore& pAirports);
    ~MapWindow();

    MapWindow(const MapWindow&) = delete;
    MapWindow& operator=(const MapWindow&) = delete;

    // Pencereyi verilen yer, stil ve anahtarlarla olusturur
    void Create(const MapWindowOptions& pOptions);
    void Destroy();
    bool IsCreated() const;  // Create sonrasi true; Destroy (ya da Shutdown) sonrasi false

    bool IsVisible() const;
    void SetVisible(bool pVisible);
    void ToggleVisible();
    void ToggleControls();
    void SetDiagnostics(bool pOn);

    // Harita modu: katman gorunurlugu mod basina saklanir (gecerli mod kaydedilip
    // yenisininki yuklenir). Komut prime/map/next_mode siradakine gecer.
    void SetMapMode(MapMode pMode);
    void NextMapMode();
    MapMode GetMapMode() const;

    // Haritada gorunen alan (bolge onbellegi komutu icin)
    GeoBounds VisibleBounds() const;

    // Alt seritte gosterilecek kisa durum yazisi (bos = yok).
    // Ornek: "Loading nav data: 120000"
    void SetStatusMessage(const std::string& pMessage);

    // Veri yukleniyor mu (zemin, havalimani ayrintisi, nav ...): harita resmi bu surece
    // hiz sinirinda (max_fps) surekli yeniden cizilir, bitince bir kez daha
    void SetDataBusy(bool pBusy);

    // Flight loop'tan her karede: pencerenin X dugmesiyle kapatildigini fark eder
    void Update(float pNow);

    // Gorunum, palet ve katman ayarlarini bellekteki ayara yazar, SettingsRevision'i artirir
    void SaveSettings();

    // --- API (api/PrimeMap.cpp) ---
    MapView& View();
    void SetGeometry(const RectI& pGeometry);
    RectI GetGeometry() const;
    void BringToFront();
    void SetPoppedOut(bool pPoppedOut);
    bool IsPoppedOut() const;
    void SetUiVisible(bool pVisible);
    bool IsUiVisible() const;
    void SetMouseInput(bool pEnabled);
    bool IsMouseInputEnabled() const;
    void SetFollowAircraft(bool pFollow);
    bool IsFollowingAircraft() const;
    void CenterOnAircraft();
    void GoToAirport(int pAirport);  // haritayi havalimanina tasir (ucaga dokunmaz), takibi kapatir
    bool IsDiagnosticsOn() const;
    void ResetLayers();              // bu modun varsayilanlari ("Defaults" dugmesi)
    void DrawingsChanged();          // cizim degisti: DrawingsRevision artar
    std::string ExportSettings();    // [view] [tools] [layers.*]
    void ImportSettings(const std::string& pText);
    // Son cizilen kareye gore (pX, pY) altindaki seyler: katman kimligi + metin (en ustteki once)
    std::vector<std::pair<std::string, std::string>> DescribeAt(int pX, int pY);
    std::string PerformanceText() const;
    uint64_t SettingsRevision() const;
    uint64_t DrawingsRevision() const;

private:
    // --- XPLM callback'leri: refcon bu nesnenin adresi ---
    static void DrawCallback(XPLMWindowID pWindow, void* pRefcon);
    static int ClickCallback(XPLMWindowID pWindow, int pX, int pY, XPLMMouseStatus pStatus, void* pRefcon);
    static int RightClickCallback(XPLMWindowID pWindow, int pX, int pY, XPLMMouseStatus pStatus,
                                  void* pRefcon);
    static void KeyCallback(XPLMWindowID pWindow, char pKey, XPLMKeyFlags pFlags, char pVirtualKey,
                            void* pRefcon, int pLosingFocus);
    static XPLMCursorStatus CursorCallback(XPLMWindowID pWindow, int pX, int pY, void* pRefcon);
    static int WheelCallback(XPLMWindowID pWindow, int pX, int pY, int pWheel, int pClicks, void* pRefcon);

    void OnDraw();
    bool PrepareCanvas();  // OpenGL hazir mi (ilk karede yukler); degilse hatayi pencereye yazar
    void CreateTextRenderer();  // ilk karede: kendi yazi atlasimiz ([text] renderer = atlas | xplane)
    void DrawTooltip(const DrawContext& pContext, const std::string& pText);  // imlecin yaninda kucuk aciklama
    int OnClick(int pX, int pY, XPLMMouseStatus pStatus);
    int OnRightClick(int pX, int pY, XPLMMouseStatus pStatus);
    int OnWheel(int pX, int pY, int pWheel, int pClicks);
    void RememberMouse(int pX, int pY);
    void OnKey(char pKey, XPLMKeyFlags pFlags, char pVirtualKey, bool pLosingFocus);

    void DrawControls(int pMouseX, int pMouseY, bool pMouseInside);
    bool HandleControlClick(int pX, int pY);  // kontrollere tiklandiysa true
    void HandleMapClick(int pX, int pY);      // haritaya (surukleme olmadan) tiklandi
    void PerformToolbarAction(ToolbarAction pAction);
    void ApplyLayerPanelHit(const LayerPanel::Hit& pHit);
    RectI PaletteBounds() const;  // cizim paletinin kalabilecegi alan (seritlerin arasi)
    void TogglePopOut();
    // "Go to airport": paneli acar/kapatir; panel acikken klavye odagi bizde (tuslar X-Plane'e gitmez)
    void SetSearchOpen(bool pOpen);
    void SetKeyboardFocus(bool pFocus);
    bool HasKeyboardFocus() const;
    ToolbarState CurrentToolbarState() const;
    RectI ShowControlsButtonRect() const;
    bool MouseInside(int* pX, int* pY) const;
    bool IsOverControls(int pX, int pY) const;
    RectD ClearArea() const;
    std::string HintText() const;
    void LogDiagnostics(float pNow, double pPixelsPerBoxel, const RectD& pScissor);
    uint64_t SceneSignature() const;  // degisince harita resmi hemen yeniden cizilir (mod, katman, cizim)

    void ApplySettings();  // bellekteki [view] [tools] [layers.*] ayarlarini uygular
    void SaveDrawings();

    LayerManager& mLayers;
    AircraftData& mAircraft;
    Settings mSettings;  // bellekte (dosya yok): mod basina katman durumu, gorunum, palet
    Drawings& mDrawings;
    uint64_t mSettingsRevision = 0;
    uint64_t mDrawingsRevision = 0;
    bool mEmbedded = true;
    bool mMouseInput = false;
    bool mXPlaneFont = false;
    bool mPassClick = false;  // fare kapali: bu basma alttaki pencereye gecirildi
    double mLastPixelsPerBoxel = 1.0;

    XPLMWindowID mWindow = nullptr;
    MapView mView;
    Gfx::Canvas mCanvas;               // tum seklin cizildigi kapi (modern OpenGL)
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
    StatusBar mStatusBar;
    LabelPlacer mLabelPlacer;          // tum katmanlarin ortak etiket seyreltmesi
    MapImageCache mImageCache;         // 0.18: harita resmi (FBO); [render] cache = 0 ile kapatilir
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
