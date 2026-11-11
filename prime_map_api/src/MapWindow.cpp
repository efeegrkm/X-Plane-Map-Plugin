// MapWindow.cpp
#include "MapWindow.h"

#include <array>
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
#include "XPLMUtilities.h"

#include "Drawings.h"
#include "LayerManager.h"
#include "Log.h"
#include "Settings.h"
#include "data/AircraftData.h"
#include "data/AirportGeometryStore.h"
#include "data/AirportSearch.h"
#include "engine/LayerTable.h"
#include "gfx/GlScreen.h"
#include "ui/UiDraw.h"

namespace
{

const char* kViewSection = "view";
const char* kToolsSection = "tools";
const char* kFontFile = "Resources/fonts/DejaVuSans.ttf";  // X-Plane kurulumunda var (lisans: Bitstream Vera)

const int kMinWidth = 400;           // pencere en kucuk boyutu (boxel)
const int kMinHeight = 260;
const int kMaxSize = 8192;
const int kEmbeddedMinSize = 50;     // Embedded pencere: host kucuk bir alan verebilir
const double kWheelZoomStep = 0.5;   // tekerlegin her "tik"i bu kadar zoom
const float kMouseTimeout = 0.25f;   // pop-out'ta imlec bu sureden eski ise pencere disinda say
const float kDiagnosticsPeriod = 2.0f;
const int kClickSlop = 4;            // basip birakmada bundan az hareket: tiklama (boxel)


// Ilk acilista bakilacak yer (ucak henuz yoksa): Turkiye
const LatLon kDefaultCenter = {39.0, 35.0};
const double kDefaultZoom = 6.0;

}  // namespace

MapWindow::MapWindow(LayerManager& pLayers, AircraftData& pAircraft, Drawings& pDrawings,
                     const AirportGeometryStore& pAirports)
    : mLayers(pLayers)
    , mAircraft(pAircraft)
    , mDrawings(pDrawings)
    , mAirports(pAirports)
    , mSearchPanel(pAirports)
{
}

MapWindow::~MapWindow()
{
    Destroy();
}

// ============================================================================
// Olusturma, gorunurluk
// ============================================================================

void MapWindow::Create(const MapWindowOptions& pOptions)
{
    if (mWindow)
    {
        return;
    }
    mEmbedded = pOptions.mEmbedded;
    mShowControls = pOptions.mUiVisible;
    mMouseInput = pOptions.mMouseInput;
    mXPlaneFont = pOptions.mXPlaneFont;
    mUseImageCache = pOptions.mImageCache;
    mImageCache.SetMaxRate(pOptions.mMaxRenderFps);
    mFloatingGeometry = pOptions.mGeometry;
    mHasFloatingGeometry = true;
    mMapMode = pOptions.mMode;
    ApplySettings();  // bos ayar: varsayilan gorunum ve modun varsayilan katmanlari

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
    // Standalone: X-Plane'in ince cercevesi (tasinir, boyutlanir). Embedded: cercevesiz; kullanici
    // tasiyamaz / boyutlandiramaz, yeri sadece host'un SetGeometry cagrisiyla degisir.
    tParams.decorateAsFloatingWindow =
        mEmbedded ? xplm_WindowDecorationNone : xplm_WindowDecorationRoundRectangle;
    tParams.layer = xplm_WindowLayerFloatingWindows;
    mWindow = XPLMCreateWindowEx(&tParams);
    if (!mWindow)
    {
        Log("ERROR: could not create the map window");
        return;
    }

    XPLMSetWindowPositioningMode(mWindow, xplm_WindowPositionFree, -1);
    if (mEmbedded)
    {
        XPLMSetWindowResizingLimits(mWindow, kEmbeddedMinSize, kEmbeddedMinSize, kMaxSize, kMaxSize);
    }
    else
    {
        XPLMSetWindowResizingLimits(mWindow, kMinWidth, kMinHeight, kMaxSize, kMaxSize);
    }
    std::string tTitle = "Map";  // SDK degistirilebilir char* istiyor
    XPLMSetWindowTitle(mWindow, tTitle.data());
    mFramePeriodRef = XPLMFindDataRef("sim/operation/misc/frame_rate_period");  // saniye/kare
    Log("map window created ({}, {},{} - {},{})", mEmbedded ? "embedded" : "standalone", mFloatingGeometry.mLeft,
        mFloatingGeometry.mTop, mFloatingGeometry.mRight, mFloatingGeometry.mBottom);

    if (pOptions.mVisible)
    {
        SetVisible(true);
    }
}

void MapWindow::Destroy()
{
    if (mText)
    {
        Ui::SetTextRenderer(nullptr);  // Ui artik bu nesneyi gostermesin
        mText.reset();
    }
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
    if (!mShowControls)
    {
        SetSearchOpen(false);  // gizli panel klavyeyi tutmasin
    }
    SaveSettings();
}

void MapWindow::SetDiagnostics(bool pOn)
{
    mDiagnostics = pOn;
    mLastDiagnosticsTime = -10.0f;
}

void MapWindow::SetDataBusy(bool pBusy)
{
    mDataBusy = pBusy;
}

uint64_t MapWindow::SceneSignature() const
{
    uint64_t tHash = mLayers.VisibilitySignature();
    auto tMix = [&tHash](uint64_t pValue)
    {
        tHash = (tHash ^ pValue) * 1099511628211ull;
    };
    tMix(static_cast<uint64_t>(mMapMode));
    tMix(static_cast<uint64_t>(mDrawings.ShapeCount()));
    tMix(mDrawings.Current() ? mDrawings.Current()->mPoints.size() + 1 : 0);
    const RectD tClear = ClearArea();  // kontroller gizlenince / serit iki satir olunca etiket alani degisir
    tMix(static_cast<uint64_t>(tClear.mTop * 16.0));
    tMix(static_cast<uint64_t>(tClear.mBottom * 16.0));
    return tHash;
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
    if (!tVisible && mSearchPanel.IsOpen())
    {
        SetSearchOpen(false);  // kapali pencere klavyeyi tutmasin
    }
    mSearchPanel.Refresh();  // havalimanlari arama sirasinda hazir olduysa
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

void MapWindow::KeyCallback(XPLMWindowID /*pWindow*/, char pKey, XPLMKeyFlags pFlags, char pVirtualKey,
                            void* pRefcon, int pLosingFocus)
{
    // Klavye odagini sadece "Go to airport" paneli acikken aliriz; diger tuslar X-Plane komutlariyla
    static_cast<MapWindow*>(pRefcon)->OnKey(pKey, pFlags, pVirtualKey, pLosingFocus != 0);
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
        mCanvas.FlushText();
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
    if (mText)
    {
        mText->SetPixelsPerBoxel(tContext.mPixelsPerBoxel);  // UI olcegi degistiyse atlas yeniden basilir
    }
    tContext.mNow = tNow;
    mLastPixelsPerBoxel = tContext.mPixelsPerBoxel;
    // Fare kapaliyken imlece bagli seyler (tooltip, cizim cizgisi) yok
    tContext.mHasCursor = mMouseInput && tMouseInside && !IsOverControls(tMouseX, tMouseY);
    tContext.mCursorX = tMouseX;
    tContext.mCursorY = tMouseY;
    mLabelPlacer.Reset(tContext.mClearArea);  // etiketler seritlerin altina girmesin
    tContext.mLabels = &mLabelPlacer;
    // 0.18: harita resmi onbellekte (MapImageCache). Atlas yazisi gerekli: XPLMDrawString X-Plane'in
    // kendi matrisleriyle cizer, FBO'ya dogru cizecegi belli degil.
    const bool tNewData = mLayers.PrepareAll();  // hazir dokular, vektorler
    bool tCached = false;
    if (mUseImageCache && mText && tBoxelToPixel)
    {
        const uint64_t tSignature = SceneSignature();
        const bool tUrgent = tSignature != mLastSignature;
        mLastSignature = tSignature;
        const bool tBusyEnded = mWasBusy && !mDataBusy;  // son gelen veri de resme girsin
        mWasBusy = mDataBusy;
        tCached = mImageCache.Draw(mCanvas, mView, *tBoxelToPixel, tNow, tUrgent, tNewData || tBusyEnded, mDataBusy,
                                   [this, &tContext, &tTheme](const MapView& pView, const RectD& pArea)
                                   {
                                       DrawContext tImage = tContext;
                                       tImage.mView = &pView;
                                       tImage.mArea = pArea;
                                       tImage.mHasCursor = false;  // imlece bagli cizim resme girmez
                                       Gfx::Mesh tBackground;
                                       tBackground.AddRect(pArea, tTheme.mBackground);
                                       mCanvas.Draw(tBackground);
                                       mLabelPlacer.Reset(tImage.mClearArea);
                                       mLayers.DrawAll(tImage, LayerPass::Cached);
                                   });
        if (!tCached && !mImageCacheWarned)
        {
            Log("map image cache (FBO) not available, drawing the map directly every frame");
            mImageCacheWarned = true;
        }
    }
    if (tCached)
    {
        mLabelPlacer.Reset(tContext.mClearArea);
        mLayers.DrawAll(tContext, LayerPass::Overlay);  // ucak, iz, yarim cizim: her karede ustte
    }
    else
    {
        mLayers.DrawAll(tContext);
    }
    if (tContext.mHasCursor)
    {
        DrawTooltip(tContext, mLayers.DescribeAt(tContext, tMouseX, tMouseY));
    }

    DrawControls(tMouseX, tMouseY, tMouseInside);
    mCanvas.FlushText();  // bekleyen yazilar (tek cizim cagrisi)

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

void MapWindow::CreateTextRenderer()
{
    if (mXPlaneFont)
    {
        Log("text: X-Plane font (XPLMDrawString), [text] renderer = xplane");
        return;
    }
    // Boyut X-Plane yazisiyla ayni: yerlesim (etiket kutulari, dugmeler) degismesin
    int tHeight = 10;
    XPLMGetFontDimensions(xplmFont_Proportional, nullptr, &tHeight, nullptr);
    std::array<char, 2048> tSystemPath = {};
    XPLMGetSystemPath(tSystemPath.data());
    const std::string tFontPath = std::string(tSystemPath.data()) + kFontFile;
    std::unique_ptr<AtlasTextRenderer> tText = std::make_unique<AtlasTextRenderer>(mCanvas, tHeight);
    std::string tError;
    if (!tText->Load(tFontPath, &tError))
    {
        Log("text: own font atlas not available ({}), using X-Plane font", tError);
        return;
    }
    mText = std::move(tText);
    Ui::SetTextRenderer(mText.get());
    Log("text: font atlas from {} (cap height {} boxels)", tFontPath, tHeight);
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
            CreateTextRenderer();
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
    return std::format("FPS {:.0f}  draw {:.2f} ms  map renders {}/s", tPeriod > 0.0f ? 1.0f / tPeriod : 0.0f,
                       mDrawMs, mImageCache.RendersPerSecond());
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
        // Kontroller gizli: Standalone'da imlec haritanin ustundeyken kucuk bir
        // "Controls" dugmesi goster; imlec cikinca sadece harita kalir. Embedded'de hic bir sey.
        if (pMouseInside && !mEmbedded)
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
    mSearchPanel.Layout(mToolbar.ButtonRect(ToolbarAction::GoToAirport));

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
    mSearchPanel.Draw(mCanvas, pMouseX, pMouseY, HasKeyboardFocus(), XPLMGetElapsedTime());
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
    tState.mSearchOpen = mSearchPanel.IsOpen();
    static const char* const kModeLabels[kMapModeCount] = {"Map: Scenery", "Map: VFR", "Map: IFR Low",
                                                           "Map: IFR High", "Map: Dark"};
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
           mModePanel.HitTest(pX, pY, &tMode) || mToolPalette.Contains(pX, pY) ||
           mSearchPanel.HitTest(pX, pY).mInside;
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
        mPaletteDrag = false;
        mPassClick = false;
        const bool tOnControls = HandleControlClick(pX, pY);
        if (!tOnControls && !mMouseInput)
        {
            // Fare kapali: basma, surukleme ve birakma hep birlikte alttaki pencereye gecer
            mPassClick = true;
            mPressOnMap = false;
            return 0;
        }
        XPLMBringWindowToFront(mWindow);
        mPressOnMap = !tOnControls;
        if (mPressOnMap)
        {
            SetKeyboardFocus(false);  // haritaya tiklandi: tuslar yine X-Plane'e (panel acik kalir)
        }
        mPressMoved = false;
        mPressX = pX;
        mPressY = pY;
        mDragLastX = pX;
        mDragLastY = pY;
        return 1;
    }

    if (mPassClick)
    {
        if (pStatus == xplm_MouseUp)
        {
            mPassClick = false;
        }
        return 0;
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
    if (!mMouseInput)
    {
        return IsOverControls(pX, pY) ? 1 : 0;  // fare kapali: haritadaki sag tik alttaki pencereye
    }
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
    if (mShowControls && mSearchPanel.HitTest(pX, pY).mInside)
    {
        return 1;  // arama panelinin ustunde zoom yok
    }
    if (mShowControls && mLayerPanel.Scroll(pX, pY, pClicks))
    {
        return 1;  // uzun katman listesini kaydir (harita zoom yapmasin)
    }
    if (!mMouseInput)
    {
        return IsOverControls(pX, pY) ? 1 : 0;  // fare kapali: tekerlek alttaki pencereye
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
        if (!mEmbedded && ShowControlsButtonRect().Contains(pX, pY))
        {
            ToggleControls();
            return true;
        }
        return false;
    }

    // Acik paneller en ustte: once onlara bak
    const AirportSearchPanel::Hit tSearchHit = mSearchPanel.HitTest(pX, pY);
    if (tSearchHit.mInside)
    {
        if (tSearchHit.mAirport >= 0)
        {
            GoToAirport(tSearchHit.mAirport);
        }
        else
        {
            SetKeyboardFocus(true);  // panelin herhangi bir yerine tiklama: yazmaya devam
        }
        return true;
    }
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
    if (pHit.mResetDefaults)
    {
        ResetLayers();
        return;
    }
    if (!pHit.mExpandKey.empty())
    {
        mLayerPanel.ToggleExpanded(pHit.mExpandKey);  // grubu / katmani ac-kapa (gorunurluk degismez)
        return;
    }
    // Paneldeki satiri API ogesine cevir; kurallar engine/LayerTable'da (API ile ayni yol)
    std::optional<PrimeMap::MapLayer> tItem;
    if (pHit.mSubLayerOwner >= 0)
    {
        tItem = PrimeMap::LayerFromKey(LayerManager::SubLayerKey(*mLayers.At(pHit.mSubLayerOwner), pHit.mSubLayer));
    }
    else if (!pHit.mToggle.empty())
    {
        tItem = PrimeMap::LayerFromKey(mLayers.At(pHit.mToggle.front())->Id());
        if (tItem && pHit.mIsGroupBox)
        {
            tItem = PrimeMap::ParentOf(*tItem);
        }
    }
    if (!tItem)
    {
        return;
    }
    if (LayerRules::Toggle(mLayers, ThemeFor(mMapMode), *tItem))
    {
        Log("layer '{}': {}", PrimeMap::LayerKey(*tItem), LayerRules::IsVisible(mLayers, *tItem) ? "on" : "off");
        SaveSettings();
    }
}

void MapWindow::ResetLayers()
{
    mLayers.ResetToDefaults(ThemeFor(mMapMode));
    Log("layers reset to the defaults of {} mode", ThemeFor(mMapMode).mId);
    SaveSettings();
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

GeoBounds MapWindow::VisibleBounds() const
{
    return mView.VisibleBounds();
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
            SetSearchOpen(false);
            break;
        case ToolbarAction::MapMode:
            mModePanel.Toggle();
            mLayerPanel.SetOpen(false);
            SetSearchOpen(false);
            break;
        case ToolbarAction::GoToAirport:
            SetSearchOpen(!mSearchPanel.IsOpen());
            mLayerPanel.SetOpen(false);
            mModePanel.SetOpen(false);
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

void MapWindow::SetSearchOpen(bool pOpen)
{
    mSearchPanel.SetOpen(pOpen);
    SetKeyboardFocus(pOpen);
}

void MapWindow::SetKeyboardFocus(bool pFocus)
{
    if (!mWindow)
    {
        return;
    }
    if (pFocus)
    {
        XPLMTakeKeyboardFocus(mWindow);
    }
    else if (HasKeyboardFocus())
    {
        XPLMTakeKeyboardFocus(nullptr);  // klavye X-Plane'e geri doner
    }
}

bool MapWindow::HasKeyboardFocus() const
{
    return mWindow && XPLMHasKeyboardFocus(mWindow) != 0;
}

void MapWindow::OnKey(char pKey, XPLMKeyFlags pFlags, char pVirtualKey, bool pLosingFocus)
{
    if (pLosingFocus || !mSearchPanel.IsOpen() || (pFlags & xplm_DownFlag) == 0)
    {
        return;  // odak gidiyor / panel kapali / tus birakildi
    }
    switch (static_cast<unsigned char>(pVirtualKey))
    {
        case XPLM_VK_ESCAPE:
            SetSearchOpen(false);
            return;
        case XPLM_VK_RETURN:
        case XPLM_VK_ENTER:
        case XPLM_VK_NUMPAD_ENT:
            if (mSearchPanel.SelectedAirport() >= 0)
            {
                GoToAirport(mSearchPanel.SelectedAirport());
            }
            return;
        case XPLM_VK_BACK:
            mSearchPanel.Backspace();
            return;
        case XPLM_VK_UP:
            mSearchPanel.MoveSelection(-1);
            return;
        case XPLM_VK_DOWN:
            mSearchPanel.MoveSelection(1);
            return;
        default:
            break;
    }
    if ((pFlags & (xplm_ControlFlag | xplm_OptionAltFlag)) == 0)
    {
        mSearchPanel.TypeCharacter(pKey);
    }
}

void MapWindow::GoToAirport(int pAirport)
{
    const AirportGeometry& tAirport = mAirports.At(pAirport);
    mFollowAircraft = false;  // takip acik kalirsa harita hemen ucaga geri doner
    mView.SetCenter(AirportCenter(tAirport));
    mView.SetZoom(ZoomToFitAirport(tAirport, mArea.Width(), mArea.Height()));
    Log("go to airport {} ({})", tAirport.mId, tAirport.mName);
    SetSearchOpen(false);
    SaveSettings();
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

void MapWindow::ApplySettings()
{
    LatLon tCenter;
    tCenter.mLat = mSettings.GetDouble(kViewSection, "center_lat", kDefaultCenter.mLat);
    tCenter.mLon = mSettings.GetDouble(kViewSection, "center_lon", kDefaultCenter.mLon);
    mView.SetCenter(tCenter);
    mView.SetZoom(mSettings.GetDouble(kViewSection, "zoom", kDefaultZoom));
    mFollowAircraft = mSettings.GetBool(kViewSection, "follow_aircraft", true);
    mMapMode = ModeFromId(mSettings.GetString(kViewSection, "mode", ThemeFor(mMapMode).mId), mMapMode);

    mToolPalette.SetVisible(mSettings.GetBool(kToolsSection, "visible", true));
    mToolPalette.SetCollapsed(mSettings.GetBool(kToolsSection, "collapsed", false));
    mToolPalette.SetOffset(mSettings.GetInt(kToolsSection, "offset_right", 8),
                           mSettings.GetInt(kToolsSection, "offset_top", 8));

    mLayers.LoadVisibility(mSettings, ThemeFor(mMapMode));
}

void MapWindow::SaveSettings()
{
    const LatLon tCenter = mView.Center();
    mSettings.SetDouble(kViewSection, "center_lat", tCenter.mLat);
    mSettings.SetDouble(kViewSection, "center_lon", tCenter.mLon);
    mSettings.SetDouble(kViewSection, "zoom", mView.Zoom());
    mSettings.SetBool(kViewSection, "follow_aircraft", mFollowAircraft);
    mSettings.SetString(kViewSection, "mode", ThemeFor(mMapMode).mId);

    mSettings.SetBool(kToolsSection, "visible", mToolPalette.IsVisible());
    mSettings.SetBool(kToolsSection, "collapsed", mToolPalette.IsCollapsed());
    mSettings.SetInt(kToolsSection, "offset_right", mToolPalette.OffsetRight());
    mSettings.SetInt(kToolsSection, "offset_top", mToolPalette.OffsetTop());

    mLayers.SaveVisibility(mSettings, ThemeFor(mMapMode));
    ++mSettingsRevision;  // host bunu gorup dosyaya yazar
}

void MapWindow::SaveDrawings()
{
    ++mDrawingsRevision;  // host bunu gorup ExportDrawings ile dosyaya yazar
}

std::string MapWindow::ExportSettings()
{
    SaveSettings();
    return mSettings.ToText({kViewSection, kToolsSection, "layers*"});
}

void MapWindow::ImportSettings(const std::string& pText)
{
    // Eski map_settings.ini aynen okunur ([window] gibi baska bolumler zararsiz)
    mSettings.MergeText(pText);
    ApplySettings();
    ++mSettingsRevision;
}

// ============================================================================
// API icin
// ============================================================================

MapView& MapWindow::View()
{
    return mView;
}

void MapWindow::SetGeometry(const RectI& pGeometry)
{
    mFloatingGeometry = pGeometry;
    mHasFloatingGeometry = true;
    if (mWindow && !XPLMWindowIsPoppedOut(mWindow))
    {
        XPLMSetWindowGeometry(mWindow, pGeometry.mLeft, pGeometry.mTop, pGeometry.mRight, pGeometry.mBottom);
    }
    mArea = pGeometry;
    mView.SetViewport(ToRectD(mArea));
}

RectI MapWindow::GetGeometry() const
{
    if (!mWindow)
    {
        return mFloatingGeometry;
    }
    RectI tGeometry;
    XPLMGetWindowGeometry(mWindow, &tGeometry.mLeft, &tGeometry.mTop, &tGeometry.mRight, &tGeometry.mBottom);
    return tGeometry;
}

void MapWindow::BringToFront()
{
    if (mWindow)
    {
        XPLMBringWindowToFront(mWindow);
    }
}

void MapWindow::SetPoppedOut(bool pPoppedOut)
{
    if (mWindow && !mEmbedded && IsPoppedOut() != pPoppedOut)
    {
        TogglePopOut();
    }
}

bool MapWindow::IsPoppedOut() const
{
    return mWindow && XPLMWindowIsPoppedOut(mWindow) != 0;
}

void MapWindow::SetUiVisible(bool pVisible)
{
    if (mShowControls != pVisible)
    {
        ToggleControls();
    }
}

bool MapWindow::IsUiVisible() const
{
    return mShowControls;
}

void MapWindow::SetMouseInput(bool pEnabled)
{
    mMouseInput = pEnabled;
    mPressOnMap = false;
}

bool MapWindow::IsMouseInputEnabled() const
{
    return mMouseInput;
}

void MapWindow::SetFollowAircraft(bool pFollow)
{
    mFollowAircraft = pFollow;
    if (pFollow)
    {
        CenterOnAircraft();
    }
    SaveSettings();
}

bool MapWindow::IsFollowingAircraft() const
{
    return mFollowAircraft;
}

bool MapWindow::IsDiagnosticsOn() const
{
    return mDiagnostics;
}

void MapWindow::DrawingsChanged()
{
    SaveDrawings();
}

std::vector<std::pair<std::string, std::string>> MapWindow::DescribeAt(int pX, int pY)
{
    DrawContext tContext;
    tContext.mView = &mView;
    tContext.mCanvas = &mCanvas;
    tContext.mTheme = &ThemeFor(mMapMode);
    tContext.mArea = ToRectD(mArea);
    tContext.mClearArea = ClearArea();
    tContext.mPixelsPerBoxel = mLastPixelsPerBoxel;
    tContext.mHasCursor = true;
    tContext.mCursorX = pX;
    tContext.mCursorY = pY;
    tContext.mLabels = &mLabelPlacer;
    return mLayers.DescribeAllAt(tContext, pX, pY);
}

uint64_t MapWindow::SettingsRevision() const
{
    return mSettingsRevision;
}

uint64_t MapWindow::DrawingsRevision() const
{
    return mDrawingsRevision;
}
