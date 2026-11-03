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

#include <string>

#include "XPLMDataAccess.h"
#include "XPLMDisplay.h"

#include "Geo.h"
#include "LabelPlacer.h"
#include "MapView.h"
#include "ScreenMath.h"
#include "ui/LayerPanel.h"
#include "ui/StatusBar.h"
#include "ui/Toolbar.h"

class AircraftData;
class Drawings;
class LayerManager;
class Settings;

class MapWindow
{
public:
    // pDrawingsPath: kullanici cizimlerinin kaydedilecegi dosya
    MapWindow(LayerManager& pLayers, AircraftData& pAircraft, Settings& pSettings, Drawings& pDrawings,
              const std::string& pDrawingsPath);
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

    // Alt seritte gosterilecek kisa durum yazisi (bos = yok).
    // Ornek: "Loading nav data: 120000"
    void SetStatusMessage(const std::string& pMessage);

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
    int OnClick(int pX, int pY, XPLMMouseStatus pStatus);
    int OnRightClick(int pX, int pY, XPLMMouseStatus pStatus);
    int OnWheel(int pX, int pY, int pWheel, int pClicks);
    void RememberMouse(int pX, int pY);

    void DrawControls(int pMouseX, int pMouseY, bool pMouseInside);
    bool HandleControlClick(int pX, int pY);  // kontrollere tiklandiysa true
    void HandleMapClick(int pX, int pY);      // haritaya (surukleme olmadan) tiklandi
    void PerformToolbarAction(ToolbarAction pAction);
    void TogglePopOut();
    void CenterOnAircraft();
    ToolbarState CurrentToolbarState() const;
    RectI ShowControlsButtonRect() const;
    bool MouseInside(int* pX, int* pY) const;
    bool IsOverControls(int pX, int pY) const;
    RectD ClearArea() const;
    std::string HintText() const;
    void LogDiagnostics(float pNow, double pPixelsPerBoxel, const RectD& pScissor);
    std::string PerformanceText() const;

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
    Toolbar mToolbar;
    LayerPanel mLayerPanel;
    StatusBar mStatusBar;
    LabelPlacer mLabelPlacer;          // tum katmanlarin ortak etiket seyreltmesi
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
