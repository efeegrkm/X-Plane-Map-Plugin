// MapWindow.cpp
#include "MapWindow.h"

#include <chrono>
#include <cstdio>
#include <format>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>

#include "XPLMGraphics.h"
#include "XPLMProcessing.h"

#include "Drawings.h"
#include "LayerManager.h"
#include "Log.h"
#include "Settings.h"
#include "data/AircraftData.h"
#include "gfx/GlScreen.h"
#include "ui/UiDraw.h"

namespace
{

const char* kWindowSection = "window";
const char* kViewSection = "view";
const char* kToolsSection = "tools";

const int kMinWidth = 400;           // pencere en kucuk boyutu (boxel)
const int kMinHeight = 260;
const int kMaxSize = 8192;
const double kWheelZoomStep = 0.5;   // tekerlegin her "tik"i bu kadar zoom
const float kMouseTimeout = 0.25f;   // pop-out'ta imlec bu sureden eski ise pencere disinda say
const float kDiagnosticsPeriod = 2.0f;
const int kClickSlop = 4;            // basip birakmada bundan az hareket: tiklama (boxel)


// Ilk acilista bakilacak yer (ucak henuz yoksa): Turkiye
const LatLon kDefaultCenter = {39.0, 35.0};
const double kDefaultZoom = 6.0;

}  // namespace

MapWindow::MapWindow(LayerManager& pLayers, AircraftData& pAircraft, Settings& pSettings, Drawings& pDrawings,
                     const std::string& pDrawingsPath)
    : mLayers(pLayers)
    , mAircraft(pAircraft)
    , mSettings(pSettings)
    , mDrawings(pDrawings)
    , mDrawingsPath(pDrawingsPath)
{
}

MapWindow::~MapWindow()
{
    Destroy();
}

// ============================================================================
// Olusturma, gorunurluk
// ============================================================================

void MapWindow::Create()
{
    if (mWindow)
    {
        return;
    }
    LoadSettings();
    LoadDrawings();

    XPLMCreateWindow_t tParams;
    std::memset(&tParams, 0, sizeof(tParams));
    tParams.structSize = sizeof(tParams);
    tParams.left = mFloatingGeometry.mLeft;
    tParams.top = mFloatingGeometry.mTop;
    tParams.right = mFloatingGeometry.mRight;
    tParams.bottom = mFloatingGeometry.mBottom;
    tParams.visible = 0;
    // SDK: tum callback'ler verilmeli, hicbiri null birakilmamali
    tParams.drawWindowFunc = DrawCallback;
    tParams.handleMouseClickFunc = ClickCallback;
    tParams.handleRightClickFunc = RightClickCallback;
    tParams.handleKeyFunc = KeyCallback;
    tParams.handleCursorFunc = CursorCallback;
    tParams.handleMouseWheelFunc = WheelCallback;
    tParams.refcon = this;
    tParams.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;  // X-Plane'in ince cercevesi
    tParams.layer = xplm_WindowLayerFloatingWindows;
    mWindow = XPLMCreateWindowEx(&tParams);
    if (!mWindow)
    {
        Log("ERROR: could not create the map window");
        return;
    }

    XPLMSetWindowPositioningMode(mWindow, xplm_WindowPositionFree, -1);
    XPLMSetWindowResizingLimits(mWindow, kMinWidth, kMinHeight, kMaxSize, kMaxSize);
    std::string tTitle = "Map";  // SDK degistirilebilir char* istiyor
    XPLMSetWindowTitle(mWindow, tTitle.data());
    mFramePeriodRef = XPLMFindDataRef("sim/operation/misc/frame_rate_period");  // saniye/kare
    Log("map window created ({},{} - {},{})", mFloatingGeometry.mLeft, mFloatingGeometry.mTop,
        mFloatingGeometry.mRight, mFloatingGeometry.mBottom);

    if (mStartVisible)
    {
        SetVisible(true);
    }
}

void MapWindow::Destroy()
{
    if (mWindow)
    {
        XPLMDestroyWindow(mWindow);
        mWindow = nullptr;
    }
}

bool MapWindow::IsVisible() const
{
    return mWindow && XPLMGetWindowIsVisible(mWindow) != 0;
}

void MapWindow::SetVisible(bool pVisible)
{
    if (!mWindow)
    {
        return;
    }
    XPLMSetWindowIsVisible(mWindow, pVisible ? 1 : 0);
    if (pVisible)
    {
        XPLMBringWindowToFront(mWindow);
    }
    mWasVisible = pVisible;
    SaveSettings();
}

void MapWindow::ToggleVisible()
{
    SetVisible(!IsVisible());
}

void MapWindow::ToggleControls()
{
    mShowControls = !mShowControls;
    SaveSettings();
}

void MapWindow::SetDiagnostics(bool pOn)
{
    mDiagnostics = pOn;
    mLastDiagnosticsTime = -10.0f;
}

void MapWindow::SetStatusMessage(const std::string& pMessage)
{
    mStatusMessage = pMessage;
}

void MapWindow::Update(float /*pNow*/)
{
    // Kullanici pencereyi cercevedeki X ile kapattiysa ayarlari kaydet
    const bool tVisible = IsVisible();
    if (mWasVisible && !tVisible)
    {
        mPressOnMap = false;
        SaveSettings();
    }
    mWasVisible = tVisible;
}

// ============================================================================
// XPLM callback'leri -> uye fonksiyonlar
// ============================================================================

void MapWindow::DrawCallback(XPLMWindowID /*pWindow*/, void* pRefcon)
{
    static_cast<MapWindow*>(pRefcon)->OnDraw();
}

int MapWindow::ClickCallback(XPLMWindowID /*pWindow*/, int pX, int pY, XPLMMouseStatus pStatus, void* pRefcon)
{
    return static_cast<MapWindow*>(pRefcon)->OnClick(pX, pY, pStatus);
}

int MapWindow::RightClickCallback(XPLMWindowID /*pWindow*/, int pX, int pY, XPLMMouseStatus pStatus,
                                  void* pRefcon)
{
    return static_cast<MapWindow*>(pRefcon)->OnRightClick(pX, pY, pStatus);
}

void MapWindow::KeyCallback(XPLMWindowID /*pWindow*/, char /*pKey*/, XPLMKeyFlags /*pFlags*/,
                            char /*pVirtualKey*/, void* /*pRefcon*/, int /*pLosingFocus*/)
{
    // Klavye odagi almiyoruz; tuslar X-Plane komutlariyla atanir.
}

XPLMCursorStatus MapWindow::CursorCallback(XPLMWindowID /*pWindow*/, int pX, int pY, void* pRefcon)
{
    static_cast<MapWindow*>(pRefcon)->RememberMouse(pX, pY);
    return xplm_CursorArrow;
}

int MapWindow::WheelCallback(XPLMWindowID /*pWindow*/, int pX, int pY, int pWheel, int pClicks, void* pRefcon)
{
    return static_cast<MapWindow*>(pRefcon)->OnWheel(pX, pY, pWheel, pClicks);
}

// ============================================================================
// Cizim
// ============================================================================

void MapWindow::OnDraw()
{
    const std::chrono::steady_clock::time_point tDrawStart = std::chrono::steady_clock::now();
    const float tNow = XPLMGetElapsedTime();

    int tLeft = 0;
    int tTop = 0;
    int tRight = 0;
    int tBottom = 0;
    XPLMGetWindowGeometry(mWindow, &tLeft, &tTop, &tRight, &tBottom);
    mArea.mLeft = tLeft;
    mArea.mTop = tTop;
    mArea.mRight = tRight;
    mArea.mBottom = tBottom;
    if (!XPLMWindowIsPoppedOut(mWindow))
    {
        mFloatingGeometry = mArea;
        mHasFloatingGeometry = true;
    }
    mView.SetViewport(ToRectD(mArea));

    // Ucak verisini cizimden hemen once oku (sim duraklatilmis olsa bile guncel)
    mAircraft.Refresh();
    if (mFollowAircraft && mAircraft.State().mValid)
    {
        mView.SetCenter(mAircraft.State().mPosition);
    }

    // Arac seridinin yuksekligi (bir/iki satir) katmanlarin "bos alanini" belirler
    const ToolbarState tToolbarState = CurrentToolbarState();
    if (mShowControls)
    {
        mToolbar.Layout(mArea, tToolbarState);
    }

    // Cizim kapisi: ilk karede OpenGL fonksiyonlari ve shader'lar yuklenir
    if (!PrepareCanvas())
    {
        return;
    }
    mCanvas.BeginFrame();
    if (!mCanvas.IsContextUsable())
    {
        // Pop-out penceresi shader'larimizi paylasmayan bir baglamda: cizim
        // yapilirsa gecersiz nesneler kullanilir. Cizme, nedenini yaz.
        if (!mContextWarned)
        {
            Log("ERROR: this OpenGL context does not share the map's shaders (popped out = {})",
                XPLMWindowIsPoppedOut(mWindow) != 0);
            mContextWarned = true;
        }
        Ui::Text(mArea.mLeft + 10, mArea.mTop - 30, "Map cannot draw in this window, use Pop in", Ui::kTextYellow);
        return;
    }

    // boxel -> piksel: glScissor icin gerekli (UI olcek %100 degilse fark eder)
    const std::optional<Affine2D> tBoxelToPixel = ReadBoxelToPixel();
    std::optional<ScissorGuard> tScissor;
    if (tBoxelToPixel)
    {
        tScissor.emplace(*tBoxelToPixel, ToRectD(mArea));  // pencere disina tasma
    }

    const MapTheme& tTheme = ThemeFor(mMapMode);
    Ui::FillRect(mCanvas, mArea, tTheme.mBackground);  // zemin verisi olmayan yer: duz renk

    int tMouseX = 0;
    int tMouseY = 0;
    const bool tMouseInside = MouseInside(&tMouseX, &tMouseY);

    DrawContext tContext;
    tContext.mView = &mView;
    tContext.mCanvas = &mCanvas;
    tContext.mTheme = &tTheme;
    tContext.mArea = ToRectD(mArea);
    tContext.mClearArea = ClearArea();
    tContext.mPixelsPerBoxel = tBoxelToPixel ? PixelsPerBoxel(*tBoxelToPixel) : 1.0;
    tContext.mNow = tNow;
    tContext.mHasCursor = tMouseInside && !IsOverControls(tMouseX, tMouseY);
    tContext.mCursorX = tMouseX;
    tContext.mCursorY = tMouseY;
    mLabelPlacer.Reset(tContext.mClearArea);  // etiketler seritlerin altina girmesin
    tContext.mLabels = &mLabelPlacer;
    mLayers.DrawAll(tContext);
    if (tContext.mHasCursor)
    {
        DrawTooltip(tContext, mLayers.DescribeAt(tContext, tMouseX, tMouseY));
    }

    DrawControls(tMouseX, tMouseY, tMouseInside);

    // Bizim cizim suremiz (yumusatilmis): teshis modunda alt seritte gorunur
    const double tDrawMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tDrawStart).count();
    mDrawMs = mDrawMs * 0.9 + tDrawMs * 0.1;

    if (mDiagnostics && tNow - mLastDiagnosticsTime >= kDiagnosticsPeriod)
    {
        mLastDiagnosticsTime = tNow;
        LogDiagnostics(tNow, tContext.mPixelsPerBoxel, tScissor ? tScissor->PixelRect() : RectD());
    }
}

void MapWindow::DrawTooltip(const DrawContext& pContext, const std::string& pText)
{
    if (pText.empty())
    {
        return;
    }
    // Imlecin sag altina; pencereden tasacaksa soluna / ustune
    const int tPadding = 5;
    const int tWidth = Ui::TextWidth(pText) + 2 * tPadding;
    const int tHeight = Ui::FontHeight() + 2 * tPadding;
    int tLeft = static_cast<int>(pContext.mCursorX) + 14;
    int tTop = static_cast<int>(pContext.mCursorY) - 14;
    if (tLeft + tWidth > pContext.mClearArea.mRight)
    {
        tLeft = static_cast<int>(pContext.mCursorX) - 8 - tWidth;
    }
    if (tTop - tHeight < pContext.mClearArea.mBottom)
    {
        tTop = static_cast<int>(pContext.mCursorY) + 8 + tHeight;
    }
    RectI tBox;
    tBox.mLeft = tLeft;
    tBox.mRight = tLeft + tWidth;
    tBox.mTop = tTop;
    tBox.mBottom = tTop - tHeight;
    Ui::FillRect(mCanvas, tBox, Ui::kPanelBg);
    Ui::FrameRect(mCanvas, tBox, Ui::kBorder);
    Ui::Text(tLeft + tPadding, tBox.mBottom + tPadding + 1, pText, Ui::kTextWhite);
}

bool MapWindow::PrepareCanvas()
{
    const bool tFirstTry = !mCanvasChecked;
    mCanvasChecked = true;
    if (mCanvas.Init())
    {
        if (tFirstTry)
        {
            Log("OpenGL ready: {}", mCanvas.ContextText());
        }
        return true;
    }
    if (tFirstTry)
    {
        Log("ERROR: map cannot draw: {}", mCanvas.ErrorText());
    }
    // Sekil cizemiyoruz ama X-Plane'in yazisi calisir: nedeni pencereye yaz
    Ui::Text(mArea.mLeft + 10, mArea.mTop - 30, "Map cannot draw: " + mCanvas.ErrorText(), Ui::kTextYellow);
    return false;
}

std::string MapWindow::PerformanceText() const
{
    const float tPeriod = mFramePeriodRef ? XPLMGetDataf(mFramePeriodRef) : 0.0f;
    return std::format("FPS {:.0f}  draw {:.2f} ms", tPeriod > 0.0f ? 1.0f / tPeriod : 0.0f, mDrawMs);
}

std::string MapWindow::HintText() const
{
    switch (mDrawings.GetMode())
    {
        case Drawings::Mode::Point:
            return "Click: add point";
        case Drawings::Mode::Route:
            return "Click: add vertex   Right click: finish route";
        case Drawings::Mode::Region:
            return "Click: add vertex   Right click: close region";
        case Drawings::Mode::Pan:
            break;
    }
    return "";
}

void MapWindow::DrawControls(int pMouseX, int pMouseY, bool pMouseInside)
{
    if (!mShowControls)
    {
        // Kontroller gizli: sadece imlec haritanin ustundeyken kucuk bir
        // "Controls" dugmesi goster; imlec cikinca sadece harita kalir.
        if (pMouseInside)
        {
            const RectI tButton = ShowControlsButtonRect();
            Ui::Button(mCanvas, tButton, "Controls", false, true, tButton.Contains(pMouseX, pMouseY));
        }
        return;
    }

    const ToolbarState tState = CurrentToolbarState();
    mLayerPanel.Layout(mToolbar.ButtonRect(ToolbarAction::Layers), mArea.mBottom + StatusBar::kHeight + 4, mLayers);
    mToolPalette.Layout(PaletteBounds(), tState);
    mModePanel.Layout(mToolbar.ButtonRect(ToolbarAction::MapMode));

    // Imlecin altindaki nokta: kontrollerin ustundeyse gosterme
    std::optional<LatLon> tCursor;
    if (pMouseInside && !IsOverControls(pMouseX, pMouseY))
    {
        tCursor = mView.BoxelToLatLon(pMouseX, pMouseY);
    }

    // Durum yazisi onceligi: yukleme mesaji > teshis (performans) > cizim ipucu
    std::string tMessage = mStatusMessage;
    if (tMessage.empty() && mDiagnostics)
    {
        tMessage = PerformanceText();
    }
    if (tMessage.empty())
    {
        tMessage = HintText();
    }

    mToolbar.Draw(mCanvas, pMouseX, pMouseY, tState);
    mStatusBar.Draw(mCanvas, mArea, mView, tCursor, tMessage);
    mToolPalette.Draw(mCanvas, pMouseX, pMouseY, tState);
    mLayerPanel.Draw(mCanvas, pMouseX, pMouseY, mLayers);  // en ustte: acilir paneller
    mModePanel.Draw(mCanvas, pMouseX, pMouseY, mMapMode);
}

RectD MapWindow::ClearArea() const
{
    RectD tClear = ToRectD(mArea);
    if (mShowControls)
    {
        tClear.mTop -= mToolbar.Height();
        tClear.mBottom += StatusBar::kHeight;
    }
    return tClear;
}

RectI MapWindow::PaletteBounds() const
{
    RectI tBounds = mArea;
    tBounds.mTop -= mToolbar.Height() + 4;
    tBounds.mBottom += StatusBar::kHeight + 4;
    tBounds.mLeft += 4;
    tBounds.mRight -= 4;
    return tBounds;
}

RectI MapWindow::ShowControlsButtonRect() const
{
    RectI tRect;
    tRect.mLeft = mArea.mLeft + 4;
    tRect.mTop = mArea.mTop - 4;
    tRect.mRight = tRect.mLeft + Ui::ButtonWidth("Controls");
    tRect.mBottom = tRect.mTop - (Toolbar::kRowHeight - 8);
    return tRect;
}

ToolbarState MapWindow::CurrentToolbarState() const
{
    ToolbarState tState;
    tState.mLayersOpen = mLayerPanel.IsOpen();
    tState.mModesOpen = mModePanel.IsOpen();
    static const char* const kModeLabels[kMapModeCount] = {"Map: Scenery", "Map: VFR", "Map: IFR Low",
                                                           "Map: IFR High"};
    tState.mModeLabel = kModeLabels[static_cast<int>(mMapMode)];
    tState.mFollowing = mFollowAircraft;
    tState.mHasAircraft = mAircraft.State().mValid;
    tState.mPoppedOut = mWindow && XPLMWindowIsPoppedOut(mWindow);
    tState.mToolsOpen = mToolPalette.IsVisible();
    tState.mMode = mDrawings.GetMode();
    tState.mCanFinish = mDrawings.Current() != nullptr;
    tState.mCanUndo = tState.mCanFinish || mDrawings.ShapeCount() > 0;
    return tState;
}

bool MapWindow::MouseInside(int* pX, int* pY) const
{
    if (mWindow && !XPLMWindowIsPoppedOut(mWindow))
    {
        // X-Plane penceresinin icinde: imlecin yeri her an okunabilir
        XPLMGetMouseLocationGlobal(pX, pY);
        return mArea.Contains(*pX, *pY);
    }
    // Disari alinmis pencere: cursor callback'ten gelen son konum
    *pX = mMouseX;
    *pY = mMouseY;
    return XPLMGetElapsedTime() - mMouseTime < kMouseTimeout && mArea.Contains(mMouseX, mMouseY);
}

bool MapWindow::IsOverControls(int pX, int pY) const
{
    if (!mShowControls)
    {
        return false;
    }
    std::optional<MapMode> tMode;
    return mToolbar.Contains(pX, pY) || mStatusBar.Contains(pX, pY) || mLayerPanel.HitTest(pX, pY, mLayers).mInside ||
           mModePanel.HitTest(pX, pY, &tMode) || mToolPalette.Contains(pX, pY);
}

void MapWindow::LogDiagnostics(float pNow, double pPixelsPerBoxel, const RectD& pScissor)
{
    int tMouseX = 0;
    int tMouseY = 0;
    const bool tInside = MouseInside(&tMouseX, &tMouseY);
    int tGlobalX = 0;
    int tGlobalY = 0;
    XPLMGetMouseLocationGlobal(&tGlobalX, &tGlobalY);
    const LatLon tCenter = mView.Center();
    Log("DIAG t={:.1f} window=({},{},{},{}) popped_out={} pixels_per_boxel={:.3f} "
        "scissor=({:.0f},{:.0f},{:.0f},{:.0f}) "
        "zoom={:.2f} center={:.5f},{:.5f} mouse=({},{},{}) mouse_global=({},{}) aircraft={} labels={} {}",
        pNow, mArea.mLeft, mArea.mTop, mArea.mRight, mArea.mBottom, XPLMWindowIsPoppedOut(mWindow),
        pPixelsPerBoxel, pScissor.mLeft, pScissor.mTop, pScissor.mRight, pScissor.mBottom, mView.Zoom(),
        tCenter.mLat, tCenter.mLon, tMouseX, tMouseY, tInside ? "inside" : "outside", tGlobalX, tGlobalY,
        mAircraft.State().mValid ? 1 : 0, mLabelPlacer.PlacedCount(), PerformanceText());
}

// ============================================================================
// Fare
// ============================================================================

void MapWindow::RememberMouse(int pX, int pY)
{
    mMouseX = pX;
    mMouseY = pY;
    mMouseTime = XPLMGetElapsedTime();
}

int MapWindow::OnClick(int pX, int pY, XPLMMouseStatus pStatus)
{
    RememberMouse(pX, pY);

    if (pStatus == xplm_MouseDown)
    {
        XPLMBringWindowToFront(mWindow);
        mPaletteDrag = false;
        mPressOnMap = !HandleControlClick(pX, pY);
        mPressMoved = false;
        mPressX = pX;
        mPressY = pY;
        mDragLastX = pX;
        mDragLastY = pY;
        return 1;
    }

    if (pStatus == xplm_MouseDrag && mPaletteDrag)
    {
        mToolPalette.MoveBy(pX - mDragLastX, pY - mDragLastY);
        mDragLastX = pX;
        mDragLastY = pY;
        return 1;
    }

    if (pStatus == xplm_MouseDrag && mPressOnMap)
    {
        // Kucuk titremeler tiklama sayilsin: esik asilana kadar kaydirma yok
        if (!mPressMoved && (std::abs(pX - mPressX) > kClickSlop || std::abs(pY - mPressY) > kClickSlop))
        {
            mPressMoved = true;
        }
        if (mPressMoved && (pX != mDragLastX || pY != mDragLastY))
        {
            mView.PanByBoxels(pX - mDragLastX, pY - mDragLastY);
            mFollowAircraft = false;  // elle kaydirinca takip biter
            mDragLastX = pX;
            mDragLastY = pY;
        }
        return 1;
    }

    if (pStatus == xplm_MouseUp)
    {
        if (mPressOnMap && !mPressMoved)
        {
            HandleMapClick(mPressX, mPressY);
        }
        if (mPaletteDrag)
        {
            SaveSettings();  // paletin yeni yeri
        }
        mPressOnMap = false;
        mPaletteDrag = false;
    }
    return 1;  // tiklamayi her zaman tuket (gecirmek fare takibini bozuyor)
}

int MapWindow::OnRightClick(int pX, int pY, XPLMMouseStatus pStatus)
{
    RememberMouse(pX, pY);
    // Sag tik: yarim kalan rota/bolgeyi bitir (tiklamayi her durumda tuket)
    if (pStatus == xplm_MouseDown && !IsOverControls(pX, pY) && mDrawings.Current())
    {
        mDrawings.Finish();
        SaveDrawings();
    }
    return 1;
}

void MapWindow::HandleMapClick(int pX, int pY)
{
    if (mDrawings.GetMode() == Drawings::Mode::Pan)
    {
        return;
    }
    mDrawings.AddVertex(mView.BoxelToLatLon(pX, pY));
    if (mDrawings.GetMode() == Drawings::Mode::Point)
    {
        SaveDrawings();  // nokta hemen bitmis bir sekil
    }
}

int MapWindow::OnWheel(int pX, int pY, int pWheel, int pClicks)
{
    RememberMouse(pX, pY);
    if (pWheel != 0)
    {
        return 1;  // yatay tekerlek: kullanmiyoruz
    }
    if (mShowControls && mLayerPanel.Scroll(pX, pY, pClicks))
    {
        return 1;  // uzun katman listesini kaydir (harita zoom yapmasin)
    }
    const double tDelta = pClicks * kWheelZoomStep;
    if (mFollowAircraft && mAircraft.State().mValid)
    {
        // Takipteyken merkez ucakta kalmali: ucagin etrafinda zoom
        mView.SetZoom(mView.Zoom() + tDelta);
    }
    else
    {
        // Imlecin altindaki nokta yerinde kalir
        mView.ZoomAt(pX, pY, tDelta);
    }
    return 1;
}

bool MapWindow::HandleControlClick(int pX, int pY)
{
    if (!mShowControls)
    {
        if (ShowControlsButtonRect().Contains(pX, pY))
        {
            ToggleControls();
            return true;
        }
        return false;
    }

    // Acik panel en ustte: once ona bak
    std::optional<MapMode> tModeHit;
    if (mModePanel.HitTest(pX, pY, &tModeHit))
    {
        if (tModeHit)
        {
            SetMapMode(*tModeHit);
            mModePanel.SetOpen(false);
        }
        return true;
    }
    const LayerPanel::Hit tPanelHit = mLayerPanel.HitTest(pX, pY, mLayers);
    if (tPanelHit.mInside)
    {
        ApplyLayerPanelHit(tPanelHit);
        return true;
    }
    const ToolPalette::Hit tPaletteHit = mToolPalette.HitTest(pX, pY, CurrentToolbarState());
    if (tPaletteHit.mInside)
    {
        if (tPaletteHit.mStartDrag)
        {
            mPaletteDrag = true;
        }
        else if (tPaletteHit.mToggleCollapse)
        {
            mToolPalette.SetCollapsed(!mToolPalette.IsCollapsed());
            SaveSettings();
        }
        else
        {
            PerformToolbarAction(tPaletteHit.mAction);
        }
        return true;
    }
    if (mToolbar.Contains(pX, pY))
    {
        PerformToolbarAction(mToolbar.HitTest(pX, pY, CurrentToolbarState()));
        return true;
    }
    return mStatusBar.Contains(pX, pY);
}

void MapWindow::ApplyLayerPanelHit(const LayerPanel::Hit& pHit)
{
    if (!pHit.mExpandKey.empty())
    {
        mLayerPanel.ToggleExpanded(pHit.mExpandKey);  // grubu / katmani ac-kapa (gorunurluk degismez)
        return;
    }
    bool tChanged = false;
    if (pHit.mSubLayerOwner >= 0)
    {
        // Alt katman: tersine cevir. Acilirsa katman da acilir (yoksa gorunmezdi);
        // son alt katman kapanirsa katman da kapanir (kutu "bos" gorunsun)
        Layer* tLayer = mLayers.At(pHit.mSubLayerOwner);
        const bool tVisible = !(tLayer->IsVisible() && tLayer->IsSubLayerVisible(pHit.mSubLayer));
        if (tVisible && !tLayer->IsVisible())
        {
            // Kapali katmanin bir alt katmani acildi: sadece o acilsin (katman "yarim"
            // gorunur), digerleri kapali kalsin; eski alt katman ayarlari geri gelmesin
            for (int tSub = 0; tSub < tLayer->SubLayerCount(); ++tSub)
            {
                tLayer->SetSubLayerVisible(tSub, false);
            }
        }
        tLayer->SetSubLayerVisible(pHit.mSubLayer, tVisible);
        if (tVisible)
        {
            tLayer->SetVisible(true);
        }
        else if (tLayer->VisibleSubLayerCount() == 0)
        {
            tLayer->SetVisible(false);
        }
        Log("layer '{}': {}", LayerManager::SubLayerKey(*tLayer, pHit.mSubLayer), tVisible ? "on" : "off");
        tChanged = true;
    }
    // Grup kutusu: bu modda varsayilan acik katmanlardan biri kapaliysa onlari ac
    // (varsayilan kapali olanlar, ornek Taxi routes, grupla acilmaz; tek tek acilir);
    // hepsi aciksa gruptaki her katmani kapat
    bool tGroupOn = false;
    std::vector<int> tToggle = pHit.mToggle;
    if (pHit.mIsGroupBox)
    {
        std::vector<int> tDefaultOn;
        for (int tIndex : pHit.mToggle)
        {
            if (LayerManager::ThemeDefault(*mLayers.At(tIndex), ThemeFor(mMapMode)))
            {
                tDefaultOn.push_back(tIndex);
            }
        }
        if (tDefaultOn.empty())
        {
            tDefaultOn = pHit.mToggle;
        }
        for (int tIndex : tDefaultOn)
        {
            tGroupOn = tGroupOn || !mLayers.At(tIndex)->IsVisible();
        }
        if (tGroupOn)
        {
            tToggle = tDefaultOn;
        }
    }
    for (int tIndex : tToggle)
    {
        Layer* tLayer = mLayers.At(tIndex);
        // Grup kutusu: hepsini ayni duruma getir; tek katman: tersine cevir
        const bool tVisible = pHit.mIsGroupBox ? tGroupOn : !tLayer->IsVisible();
        if (tLayer->IsVisible() != tVisible)
        {
            tLayer->SetVisible(tVisible);
            Log("layer '{}': {}", tLayer->Id(), tVisible ? "on" : "off");
        }
        // Butun alt katmanlari kapali bir katman acilirsa hepsini ac (bos katman gostermesin)
        if (tVisible && tLayer->SubLayerCount() > 0 && tLayer->VisibleSubLayerCount() == 0)
        {
            for (int tSub = 0; tSub < tLayer->SubLayerCount(); ++tSub)
            {
                tLayer->SetSubLayerVisible(tSub, true);
            }
        }
        tChanged = true;
    }
    if (tChanged)
    {
        SaveSettings();
    }
}

void MapWindow::SetMapMode(MapMode pMode)
{
    if (pMode == mMapMode)
    {
        return;
    }
    mLayers.SaveVisibility(mSettings, ThemeFor(mMapMode));  // eski modun katmanlari
    mMapMode = pMode;
    mLayers.LoadVisibility(mSettings, ThemeFor(mMapMode));
    Log("map mode: {}", ThemeFor(mMapMode).mId);
    SaveSettings();
}

void MapWindow::NextMapMode()
{
    SetMapMode(NextMode(mMapMode));
}

MapMode MapWindow::GetMapMode() const
{
    return mMapMode;
}

void MapWindow::PerformToolbarAction(ToolbarAction pAction)
{
    switch (pAction)
    {
        case ToolbarAction::Layers:
            mLayerPanel.Toggle();
            mModePanel.SetOpen(false);
            break;
        case ToolbarAction::MapMode:
            mModePanel.Toggle();
            mLayerPanel.SetOpen(false);
            break;
        case ToolbarAction::FollowAircraft:
            mFollowAircraft = !mFollowAircraft;
            if (mFollowAircraft)
            {
                CenterOnAircraft();
            }
            break;
        case ToolbarAction::CenterAircraft:
            CenterOnAircraft();
            break;
        case ToolbarAction::DrawTools:
            mToolPalette.SetVisible(!mToolPalette.IsVisible());
            if (!mToolPalette.IsVisible() && mDrawings.GetMode() != Drawings::Mode::Pan)
            {
                mDrawings.SetMode(Drawings::Mode::Pan);  // gizli palette cizim modunda kalinmasin
            }
            SaveDrawings();
            SaveSettings();
            break;
        case ToolbarAction::ModePan:
            mDrawings.SetMode(Drawings::Mode::Pan);
            SaveDrawings();  // mod degisince yarim cizim bitirilmis olabilir
            break;
        case ToolbarAction::ModePoint:
            mDrawings.SetMode(Drawings::Mode::Point);
            SaveDrawings();
            break;
        case ToolbarAction::ModeRoute:
            mDrawings.SetMode(Drawings::Mode::Route);
            SaveDrawings();
            break;
        case ToolbarAction::ModeRegion:
            mDrawings.SetMode(Drawings::Mode::Region);
            SaveDrawings();
            break;
        case ToolbarAction::Finish:
            mDrawings.Finish();
            SaveDrawings();
            break;
        case ToolbarAction::Undo:
            mDrawings.Undo();
            SaveDrawings();
            break;
        case ToolbarAction::Clear:
            mDrawings.Clear();
            SaveDrawings();
            break;
        case ToolbarAction::PopOut:
            TogglePopOut();
            break;
        case ToolbarAction::HideControls:
            ToggleControls();
            break;
        case ToolbarAction::None:
            break;
    }
}

void MapWindow::TogglePopOut()
{
    if (XPLMWindowIsPoppedOut(mWindow))
    {
        // X-Plane penceresine geri al, son bilinen yerine koy
        XPLMSetWindowPositioningMode(mWindow, xplm_WindowPositionFree, -1);
        if (mHasFloatingGeometry)
        {
            XPLMSetWindowGeometry(mWindow, mFloatingGeometry.mLeft, mFloatingGeometry.mTop,
                                  mFloatingGeometry.mRight, mFloatingGeometry.mBottom);
        }
        Log("window moved back into X-Plane");
    }
    else
    {
        XPLMSetWindowPositioningMode(mWindow, xplm_WindowPopOut, -1);
        Log("window popped out into its own OS window");
    }
    mPressOnMap = false;
}

void MapWindow::CenterOnAircraft()
{
    if (mAircraft.State().mValid)
    {
        mView.SetCenter(mAircraft.State().mPosition);
    }
}

// ============================================================================
// Ayarlar ve cizim dosyasi
// ============================================================================

void MapWindow::LoadSettings()
{
    RectI tSaved;
    tSaved.mLeft = mSettings.GetInt(kWindowSection, "left", 0);
    tSaved.mTop = mSettings.GetInt(kWindowSection, "top", 0);
    tSaved.mRight = mSettings.GetInt(kWindowSection, "right", 0);
    tSaved.mBottom = mSettings.GetInt(kWindowSection, "bottom", 0);
    if (IsGeometryUsable(tSaved))
    {
        mFloatingGeometry = tSaved;
    }
    else
    {
        mFloatingGeometry = DefaultGeometry();
    }
    mHasFloatingGeometry = true;
    mStartVisible = mSettings.GetBool(kWindowSection, "visible", false);

    LatLon tCenter;
    tCenter.mLat = mSettings.GetDouble(kViewSection, "center_lat", kDefaultCenter.mLat);
    tCenter.mLon = mSettings.GetDouble(kViewSection, "center_lon", kDefaultCenter.mLon);
    mView.SetCenter(tCenter);
    mView.SetZoom(mSettings.GetDouble(kViewSection, "zoom", kDefaultZoom));
    mFollowAircraft = mSettings.GetBool(kViewSection, "follow_aircraft", true);
    mShowControls = mSettings.GetBool(kViewSection, "show_controls", true);
    mMapMode = ModeFromId(mSettings.GetString(kViewSection, "mode", "scenery"), MapMode::Scenery);

    mToolPalette.SetVisible(mSettings.GetBool(kToolsSection, "visible", true));
    mToolPalette.SetCollapsed(mSettings.GetBool(kToolsSection, "collapsed", false));
    mToolPalette.SetOffset(mSettings.GetInt(kToolsSection, "offset_right", 8),
                           mSettings.GetInt(kToolsSection, "offset_top", 8));

    mLayers.LoadVisibility(mSettings, ThemeFor(mMapMode));
}

void MapWindow::SaveSettings()
{
    if (mHasFloatingGeometry)
    {
        mSettings.SetInt(kWindowSection, "left", mFloatingGeometry.mLeft);
        mSettings.SetInt(kWindowSection, "top", mFloatingGeometry.mTop);
        mSettings.SetInt(kWindowSection, "right", mFloatingGeometry.mRight);
        mSettings.SetInt(kWindowSection, "bottom", mFloatingGeometry.mBottom);
    }
    mSettings.SetBool(kWindowSection, "visible", IsVisible());

    const LatLon tCenter = mView.Center();
    mSettings.SetDouble(kViewSection, "center_lat", tCenter.mLat);
    mSettings.SetDouble(kViewSection, "center_lon", tCenter.mLon);
    mSettings.SetDouble(kViewSection, "zoom", mView.Zoom());
    mSettings.SetBool(kViewSection, "follow_aircraft", mFollowAircraft);
    mSettings.SetBool(kViewSection, "show_controls", mShowControls);
    mSettings.SetString(kViewSection, "mode", ThemeFor(mMapMode).mId);

    mSettings.SetBool(kToolsSection, "visible", mToolPalette.IsVisible());
    mSettings.SetBool(kToolsSection, "collapsed", mToolPalette.IsCollapsed());
    mSettings.SetInt(kToolsSection, "offset_right", mToolPalette.OffsetRight());
    mSettings.SetInt(kToolsSection, "offset_top", mToolPalette.OffsetTop());

    mLayers.SaveVisibility(mSettings, ThemeFor(mMapMode));

    if (!mSettings.Save())
    {
        Log("WARNING: could not write the settings file: {}", mSettings.Path());
    }
}

void MapWindow::LoadDrawings()
{
    std::ifstream tFile(mDrawingsPath);
    if (!tFile)
    {
        return;  // henuz hic cizim kaydedilmemis
    }
    std::ostringstream tText;
    tText << tFile.rdbuf();
    const int tCount = mDrawings.Deserialize(tText.str());
    Log("drawings loaded: {} shapes from {}", tCount, mDrawingsPath);
}

void MapWindow::SaveDrawings()
{
    std::ofstream tFile(mDrawingsPath, std::ios::trunc);
    if (!tFile)
    {
        Log("WARNING: could not write the drawings file: {}", mDrawingsPath);
        return;
    }
    tFile << mDrawings.Serialize();
}

RectI MapWindow::DefaultGeometry() const
{
    // Ana X-Plane penceresinin ortasinda, ekranin %70'i (en fazla 1000x700)
    int tLeft = 0;
    int tTop = 0;
    int tRight = 0;
    int tBottom = 0;
    XPLMGetScreenBoundsGlobal(&tLeft, &tTop, &tRight, &tBottom);
    int tWidth = (tRight - tLeft) * 7 / 10;
    int tHeight = (tTop - tBottom) * 7 / 10;
    if (tWidth > 1000)
    {
        tWidth = 1000;
    }
    if (tHeight > 700)
    {
        tHeight = 700;
    }
    if (tWidth < kMinWidth)
    {
        tWidth = kMinWidth;
    }
    if (tHeight < kMinHeight)
    {
        tHeight = kMinHeight;
    }
    RectI tGeometry;
    tGeometry.mLeft = (tLeft + tRight) / 2 - tWidth / 2;
    tGeometry.mRight = tGeometry.mLeft + tWidth;
    tGeometry.mBottom = (tTop + tBottom) / 2 - tHeight / 2;
    tGeometry.mTop = tGeometry.mBottom + tHeight;
    return tGeometry;
}

bool MapWindow::IsGeometryUsable(const RectI& pGeometry) const
{
    if (pGeometry.Width() < kMinWidth || pGeometry.Height() < kMinHeight)
    {
        return false;
    }
    // Pencerenin baslik cubugu ekranda kalmali (yoksa kullanici tutup tasiyamaz).
    // Kaydedilen yer, monitor sokulmus veya cozunurluk degismisse ekran disinda kalabilir.
    int tLeft = 0;
    int tTop = 0;
    int tRight = 0;
    int tBottom = 0;
    XPLMGetScreenBoundsGlobal(&tLeft, &tTop, &tRight, &tBottom);
    const int tTitleBar = 30;
    const bool tTitleVisible = pGeometry.mTop + tTitleBar <= tTop && pGeometry.mTop >= tBottom + 20;
    const bool tHorizontalVisible = pGeometry.mRight >= tLeft + 100 && pGeometry.mLeft <= tRight - 100;
    return tTitleVisible && tHorizontalVisible;
}
