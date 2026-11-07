// MapWindow.h
// Haritanin kendi penceresi (XPLMCreateWindowEx). Pencerenin tamami harita;
// kontroller haritanin ustunde, ince ve yari saydam.
//
// Gorevleri:
//   - pencereyi olusturmak, gostermek/gizlemek, X-Plane disina almak (pop-out)
//   - fare: surukleme kaydirir (her modda), tekerlek imlecin etrafinda zoom;
//     cizim modlarinda tiklama kose ekler, sag tik rota/bolgeyi bitirir
//   - her karede cizim sirasi: zemin rengi -> katmanlar -> kontroller
//   - pencere yeri/boyutu, gorunum ve katmanlarin ayar dosyasina, kullanici
//     cizimlerinin cizim dosyasina kaydi
#pragma once

#include <memory>
#include <string>

#include "XPLMDataAccess.h"
#include "XPLMDisplay.h"

#include "Geo.h"
#include "LabelPlacer.h"
#include "MapImageCache.h"
#include "MapView.h"
#include "ScreenMath.h"
#include "gfx/Canvas.h"
#include "MapTheme.h"
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
class Settings;

struct DrawContext;

class MapWindow
{
public:
    // pDrawingsPath: kullanici cizimlerinin kaydedilecegi dosya; pAirports: "Go to airport" aramasi
    MapWindow(LayerManager& pLayers, AircraftData& pAircraft, Settings& pSettings, Drawings& pDrawings,
              const std::string& pDrawingsPath, const AirportGeometryStore& pAirports);
    ~MapWindow();

    MapWindow(const MapWindow&) = delete;
    MapWindow& operator=(const MapWindow&) = delete;

    // Pencereyi ayar dosyasindaki yer/boyutla olusturur, cizimleri yukler.
    // Son kapanista acik birakildiysa yine acik baslar.
    void Create();
    void Destroy();

    bool IsVisible() const;
    void SetVisible(bool pVisible);
    void ToggleVisible();
    void ToggleControls();
    void SetDiagnostics(bool pOn);

    // Harita modu: katman gorunurlugu mod basina saklanir (gecerli mod kaydedilip
    // yenisininki yuklenir). Komut ozdev/map/next_mode siradakine gecer.
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

    // Pencere, gorunum ve katman ayarlarini dosyaya yazar
    void SaveSettings();

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
    void CenterOnAircraft();
    // "Go to airport": paneli acar/kapatir; panel acikken klavye odagi bizde (tuslar X-Plane'e gitmez)
    void SetSearchOpen(bool pOpen);
    void SetKeyboardFocus(bool pFocus);
    bool HasKeyboardFocus() const;
    void GoToAirport(int pAirport);  // haritayi havalimanina tasir (ucaga dokunmaz), takibi kapatir
    ToolbarState CurrentToolbarState() const;
    RectI ShowControlsButtonRect() const;
    bool MouseInside(int* pX, int* pY) const;
    bool IsOverControls(int pX, int pY) const;
    RectD ClearArea() const;
    std::string HintText() const;
    void LogDiagnostics(float pNow, double pPixelsPerBoxel, const RectD& pScissor);
    std::string PerformanceText() const;
    uint64_t SceneSignature() const;  // degisince harita resmi hemen yeniden cizilir (mod, katman, cizim)

    void LoadSettings();
    void LoadDrawings();
    void SaveDrawings();
    RectI DefaultGeometry() const;
    bool IsGeometryUsable(const RectI& pGeometry) const;

    LayerManager& mLayers;
    AircraftData& mAircraft;
    Settings& mSettings;
    Drawings& mDrawings;
    std::string mDrawingsPath;

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
    bool mStartVisible = false;        // ayar dosyasina gore acik baslasin mi
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
