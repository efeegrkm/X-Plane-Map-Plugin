// MapWindow.h
// Haritanin kendi penceresi (XPLMCreateWindowEx). Pencerenin tamami harita;
// kontroller haritanin ustunde, ince ve yari saydam.
//
// Gorevleri:
//   - pencereyi olusturmak, gostermek/gizlemek, X-Plane disina almak (pop-out)
//   - fare: sol surukleme kaydirir, tekerlek imlecin etrafinda zoom yapar
//   - her karede cizim sirasi: zemin rengi -> katmanlar -> kontroller
//   - pencere yeri/boyutu, gorunum (merkez, zoom) ve katmanlarin ayar
//     dosyasina kaydi
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
class LayerManager;
class Settings;

class MapWindow
{
public:
    MapWindow(LayerManager& pLayers, AircraftData& pAircraft, Settings& pSettings);
    ~MapWindow();

    MapWindow(const MapWindow&) = delete;
    MapWindow& operator=(const MapWindow&) = delete;

    // Pencereyi ayar dosyasindaki yer/boyutla olusturur. Son kapanista
    // acik birakildiysa yine acik baslar.
    void Create();
    void Destroy();

    bool IsVisible() const;
    void SetVisible(bool pVisible);
    void ToggleVisible();
    void ToggleControls();
    void SetDiagnostics(bool pOn);

    // Alt seritte gosterilecek kisa durum yazisi (bos = yok).
    // Ornek: "Nav verisi yukleniyor: 120000"
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
    int OnWheel(int pX, int pY, int pWheel, int pClicks);
    void RememberMouse(int pX, int pY);

    void DrawControls(int pMouseX, int pMouseY, bool pMouseInside);
    bool HandleControlClick(int pX, int pY);  // kontrollere tiklandiysa true
    void PerformToolbarAction(ToolbarAction pAction);
    void TogglePopOut();
    void CenterOnAircraft();
    ToolbarState CurrentToolbarState() const;
    RectI ShowControlsButtonRect() const;
    bool MouseInside(int* pX, int* pY) const;
    RectD ClearArea() const;
    void LogDiagnostics(float pNow, double pPixelsPerBoxel, const RectD& pScissor);
    std::string PerformanceText() const;

    void LoadSettings();
    RectI DefaultGeometry() const;
    bool IsGeometryUsable(const RectI& pGeometry) const;

    LayerManager& mLayers;
    AircraftData& mAircraft;
    Settings& mSettings;

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

    bool mDragging = false;
    int mDragLastX = 0;
    int mDragLastY = 0;

    // Imlecin son bilinen yeri. Pencere disari alinmissa (pop-out)
    // XPLMGetMouseLocationGlobal'a guvenmiyoruz; cursor callback'in
    // verdigi konumu ve zamanini kullaniyoruz.
    int mMouseX = 0;
    int mMouseY = 0;
    float mMouseTime = -10.0f;
};
