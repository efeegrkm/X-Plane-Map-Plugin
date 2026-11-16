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
const char* kRenderSection = "render";
const char* kTextSection = "text";
const char* kFontFile = "Resources/fonts/DejaVuSans.ttf";  // X-Plane kurulumunda var (lisans: Bitstream Vera)

const int kMinWidth = 400;           // pencere en kucuk boyutu (boxel)
const int kMinHeight = 260;
const int kMaxSize = 8192;
const int kEmbeddedMinSize = 50;     // Embedded pencere: host kucuk bir alan verebilir
const float kMouseTimeout = 0.25f;   // pop-out'ta imlec bu sureden eski ise pencere disinda say
const float kDiagnosticsPeriod = 2.0f;
const int kClickSlop = 4;            // basip birakmada bundan az hareket: tiklama (boxel)

// Ilk acilista bakilacak yer (ucak henuz yoksa): Turkiye
const LatLon kDefaultCenter = {39.0, 35.0};
const double kDefaultZoom = 6.0;

}  // namespace

XMapWindow::XMapWindow(LayerManager& pLayers, AircraftData& pAircraft, Drawings& pDrawings,
                     const AirportGeometryStore& pAirports)
    : mLayers(pLayers)
    , mAircraft(pAircraft)
    , mDrawings(pDrawings)
    , mAirports(pAirports)
    , mSearchPanel(pAirports)
{
    // Baslangic durumu (pencere acilmadan da gecerli): varsayilan gorunum ve modun varsayilan katmanlari
    mView.setCenter(kDefaultCenter);
    mView.setZoom(kDefaultZoom);
    mLayers.resetToDefaults(themeFor(mMapMode));
}

XMapWindow::~XMapWindow()
{
    destroy();
}

void XMapWindow::applyOptions(const MapWindowOptions& pOptions)
{
    if (!mWindow)
    {
        mEmbedded = pOptions.mEmbedded;
    }
    mShowControls = pOptions.mUiVisible;
    mMouseInput = pOptions.mMouseInput;
    setXPlaneFont(pOptions.mXPlaneFont);
    mUseImageCache = pOptions.mImageCache;
    setMaxRenderFps(pOptions.mMaxRenderFps);
    setGeometry(pOptions.mGeometry);
    setMapMode(pOptions.mMode);
    mWantVisible = pOptions.mVisible;
}

void XMapWindow::create(const MapWindowOptions& pOptions)
{
    if (mWindow)
    {
        return;
    }
    applyOptions(pOptions);
    create();
}

void XMapWindow::create()
{
    if (mWindow)
    {
        return;
    }
    mImageCache.setMaxRate(mMaxRenderFps);
    mImageCache.invalidate();

    XPLMCreateWindow_t tParams;
    std::memset(&tParams, 0, sizeof(tParams));
    tParams.structSize = sizeof(tParams);
    tParams.left = mFloatingGeometry.mLeft;
    tParams.top = mFloatingGeometry.mTop;
    tParams.right = mFloatingGeometry.mRight;
    tParams.bottom = mFloatingGeometry.mBottom;
    tParams.visible = 0;
    // SDK: tum callback'ler verilmeli, hicbiri null birakilmamali
    tParams.drawWindowFunc = drawCallback;
    tParams.handleMouseClickFunc = clickCallback;
    tParams.handleRightClickFunc = rightClickCallback;
    tParams.handleKeyFunc = keyCallback;
    tParams.handleCursorFunc = cursorCallback;
    tParams.handleMouseWheelFunc = wheelCallback;
    tParams.refcon = this;
    // Standalone: X-Plane'in ince cercevesi (tasinir, boyutlanir). Embedded: cercevesiz; kullanici
    // tasiyamaz / boyutlandiramaz, yeri sadece host'un setGeometry cagrisiyla degisir.
    tParams.decorateAsFloatingWindow =
        mEmbedded ? xplm_WindowDecorationNone : xplm_WindowDecorationRoundRectangle;
    tParams.layer = xplm_WindowLayerFloatingWindows;
    mWindow = XPLMCreateWindowEx(&tParams);
    if (!mWindow)
    {
        log("ERROR: could not create the map window");
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
    std::string tTitle = mTitle;  // SDK degistirilebilir char* istiyor
    XPLMSetWindowTitle(mWindow, tTitle.data());
    mFramePeriodRef = XPLMFindDataRef("sim/operation/misc/frame_rate_period");  // saniye/kare
    log("map window created ({}, {},{} - {},{})", mEmbedded ? "embedded" : "standalone", mFloatingGeometry.mLeft,
        mFloatingGeometry.mTop, mFloatingGeometry.mRight, mFloatingGeometry.mBottom);

    if (mWantVisible)
    {
        setVisible(true);
    }
}

void XMapWindow::useOwnText()
{
    // Ui'nin yazi cizicisi surecte tek bir isaretci: her pencere kendi cizim / tiklama isinin
    // basinda kendisininkini secer (yoksa baska haritanin atlasi, baska Canvas'a yazardi)
    Ui::setTextRenderer(mText.get());
}

bool XMapWindow::isEmbedded() const
{
    return mEmbedded;
}

void XMapWindow::setEmbedded(bool pEmbedded)
{
    if (mWindow)
    {
        log("WARNING: window style cannot change while the map is open (close it first)");
        return;
    }
    mEmbedded = pEmbedded;
}

bool XMapWindow::wantsVisible() const
{
    return mWantVisible;
}

void XMapWindow::setTitle(const std::string& pTitle)
{
    mTitle = pTitle.empty() ? "XMap" : pTitle;
    if (mWindow)
    {
        std::string tTitle = mTitle;
        XPLMSetWindowTitle(mWindow, tTitle.data());
    }
}

const std::string& XMapWindow::title() const
{
    return mTitle;
}

void XMapWindow::setImageCacheEnabled(bool pEnabled)
{
    mUseImageCache = pEnabled;
    mImageCache.invalidate();
    ++mSettingsRevision;
}

bool XMapWindow::isImageCacheEnabled() const
{
    return mUseImageCache;
}

void XMapWindow::setMaxRenderFps(int pFps)
{
    mMaxRenderFps = pFps < 1 ? 1 : (pFps > 240 ? 240 : pFps);
    mImageCache.setMaxRate(mMaxRenderFps);
    ++mSettingsRevision;
}

int XMapWindow::maxRenderFps() const
{
    return mMaxRenderFps;
}

void XMapWindow::setXPlaneFont(bool pXPlaneFont)
{
    if (mXPlaneFont == pXPlaneFont)
    {
        return;
    }
    mXPlaneFont = pXPlaneFont;
    ++mSettingsRevision;
    if (mCanvasChecked)
    {
        // Pencere zaten ciziyor: yazi cizici bir sonraki karede yeniden kurulur
        Ui::setTextRenderer(nullptr);
        mText.reset();
        mTextPending = true;
    }
}

bool XMapWindow::usesXPlaneFont() const
{
    return mXPlaneFont;
}

void XMapWindow::setZoomStep(double pStep)
{
    mZoomStep = pStep;
    ++mSettingsRevision;
}

double XMapWindow::zoomStep() const
{
    return mZoomStep;
}

void XMapWindow::setPanStep(double pFraction)
{
    mPanStep = pFraction;
    ++mSettingsRevision;
}

double XMapWindow::panStep() const
{
    return mPanStep;
}

void XMapWindow::setZoomLimits(double pMinZoom, double pMaxZoom)
{
    mView.setZoomLimits(pMinZoom, pMaxZoom);
    ++mSettingsRevision;
}

Settings& XMapWindow::settingsStore()
{
    return mSettings;
}

void XMapWindow::invalidateImage()
{
    mImageCache.invalidate();
}

bool XMapWindow::isCreated() const
{
    return mWindow != nullptr;
}

void XMapWindow::destroy()
{
    if (mWindow)
    {
        mWantVisible = XPLMGetWindowIsVisible(mWindow) != 0;  // yeniden acilirsa ayni gorunurlukle
        if (hasKeyboardFocus())
        {
            XPLMTakeKeyboardFocus(nullptr);  // kapanan pencere klavyeyi tutmasin
        }
        mSearchPanel.setOpen(false);
        XPLMDestroyWindow(mWindow);
        mWindow = nullptr;
        mPressOnMap = false;
    }
    // Yazi atlasi ve Canvas nesnede kalir (pencere yeniden acilabilir); Ui baska pencereninkini
    // gosteriyor olabilir: sadece bizimkiyse birak
    if (mText && Ui::currentTextRenderer() == mText.get())
    {
        Ui::setTextRenderer(nullptr);
    }
}

bool XMapWindow::isVisible() const
{
    return mWindow && XPLMGetWindowIsVisible(mWindow) != 0;
}

void XMapWindow::setVisible(bool pVisible)
{
    mWantVisible = pVisible;
    if (!mWindow)
    {
        ++mSettingsRevision;  // pencere acilinca bu gorunurlukle acilir
        return;
    }
    XPLMSetWindowIsVisible(mWindow, pVisible ? 1 : 0);
    if (pVisible)
    {
        XPLMBringWindowToFront(mWindow);
    }
    mWasVisible = pVisible;
    saveSettings();
}

void XMapWindow::toggleVisible()
{
    setVisible(!isVisible());
}

void XMapWindow::toggleControls()
{
    mShowControls = !mShowControls;
    if (!mShowControls)
    {
        setSearchOpen(false);  // gizli panel klavyeyi tutmasin
    }
    saveSettings();
}

void XMapWindow::setDiagnostics(bool pOn)
{
    mDiagnostics = pOn;
    mLastDiagnosticsTime = -10.0f;
}

void XMapWindow::setDataBusy(bool pBusy)
{
    mDataBusy = pBusy;
}

uint64_t XMapWindow::sceneSignature() const
{
    uint64_t tHash = mLayers.visibilitySignature();
    auto tMix = [&tHash](uint64_t pValue)
    {
        tHash = (tHash ^ pValue) * 1099511628211ull;
    };
    tMix(static_cast<uint64_t>(mMapMode));
    // API katman renkleri: resim onbellekli haritada eski renk kalmasin
    for (const auto& [tLayer, tColor] : mLayerColors)
    {
        tMix(static_cast<uint64_t>(static_cast<int>(tLayer)));
        tMix(static_cast<uint64_t>(static_cast<int>(tColor.mR * 65535.0f)) << 32 |
             static_cast<uint64_t>(static_cast<int>(tColor.mG * 65535.0f)) << 16 |
             static_cast<uint64_t>(static_cast<int>(tColor.mB * 65535.0f)));
    }
    tMix(static_cast<uint64_t>(mDrawings.shapeCount()));
    tMix(mDrawings.current() ? mDrawings.current()->mPoints.size() + 1 : 0);
    const RectD tClear = clearArea();  // kontroller gizlenince / serit iki satir olunca etiket alani degisir
    tMix(static_cast<uint64_t>(tClear.mTop * 16.0));
    tMix(static_cast<uint64_t>(tClear.mBottom * 16.0));
    return tHash;
}

void XMapWindow::setStatusMessage(const std::string& pMessage)
{
    mStatusMessage = pMessage;
}

void XMapWindow::update(float /*pNow*/)
{
    // Kullanici pencereyi cercevedeki X ile kapattiysa ayarlari kaydet
    const bool tVisible = isVisible();
    if (mWasVisible && !tVisible)
    {
        mPressOnMap = false;
        saveSettings();
    }
    mWasVisible = tVisible;
    if (!tVisible && mSearchPanel.isOpen())
    {
        setSearchOpen(false);  // kapali pencere klavyeyi tutmasin
    }
    mSearchPanel.refresh();  // havalimanlari arama sirasinda hazir olduysa
}

void XMapWindow::drawCallback(XPLMWindowID /*pWindow*/, void* pRefcon)
{
    static_cast<XMapWindow*>(pRefcon)->onDraw();
}

int XMapWindow::clickCallback(XPLMWindowID /*pWindow*/, int pX, int pY, XPLMMouseStatus pStatus, void* pRefcon)
{
    return static_cast<XMapWindow*>(pRefcon)->onClick(pX, pY, pStatus);
}

int XMapWindow::rightClickCallback(XPLMWindowID /*pWindow*/, int pX, int pY, XPLMMouseStatus pStatus,
                                  void* pRefcon)
{
    return static_cast<XMapWindow*>(pRefcon)->onRightClick(pX, pY, pStatus);
}

void XMapWindow::keyCallback(XPLMWindowID /*pWindow*/, char pKey, XPLMKeyFlags pFlags, char pVirtualKey,
                            void* pRefcon, int pLosingFocus)
{
    // Klavye odagini sadece "Go to airport" paneli acikken aliriz; diger tuslar X-Plane komutlariyla
    static_cast<XMapWindow*>(pRefcon)->onKey(pKey, pFlags, pVirtualKey, pLosingFocus != 0);
}

XPLMCursorStatus XMapWindow::cursorCallback(XPLMWindowID /*pWindow*/, int pX, int pY, void* pRefcon)
{
    static_cast<XMapWindow*>(pRefcon)->rememberMouse(pX, pY);
    return xplm_CursorArrow;
}

int XMapWindow::wheelCallback(XPLMWindowID /*pWindow*/, int pX, int pY, int pWheel, int pClicks, void* pRefcon)
{
    return static_cast<XMapWindow*>(pRefcon)->onWheel(pX, pY, pWheel, pClicks);
}

void XMapWindow::onDraw()
{
    const std::chrono::steady_clock::time_point tDrawStart = std::chrono::steady_clock::now();
    const float tNow = XPLMGetElapsedTime();
    useOwnText();  // coklu harita: yazilar bu pencerenin atlasina / Canvas'ina

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
    mView.setViewport(toRectD(mArea));

    // Ucak verisini cizimden hemen once oku (sim duraklatilmis olsa bile guncel)
    mAircraft.refresh();
    if (mFollowAircraft && mAircraft.state().mValid)
    {
        mView.setCenter(mAircraft.state().mPosition);
    }

    // Arac seridinin yuksekligi (bir/iki satir) katmanlarin "bos alanini" belirler
    const ToolbarState tToolbarState = currentToolbarState();
    if (mShowControls)
    {
        mToolbar.layout(mArea, tToolbarState);
    }

    // Cizim kapisi: ilk karede openGL fonksiyonlari ve shader'lar yuklenir
    if (!prepareCanvas())
    {
        return;
    }
    if (mTextPending)
    {
        mTextPending = false;
        createTextRenderer();  // setXPlaneFont acik pencerede cagrildi
    }
    mCanvas.beginFrame();
    if (!mCanvas.isContextUsable())
    {
        // Pop-out penceresi shader'larimizi paylasmayan bir baglamda: cizim
        // yapilirsa gecersiz nesneler kullanilir. Cizme, nedenini yaz.
        if (!mContextWarned)
        {
            log("ERROR: this openGL context does not share the map's shaders (popped out = {})",
                XPLMWindowIsPoppedOut(mWindow) != 0);
            mContextWarned = true;
        }
        Ui::text(mArea.mLeft + 10, mArea.mTop - 30, "XMap cannot draw in this window, use Pop in", Ui::kTextYellow);
        mCanvas.flushText();
        return;
    }

    // boxel -> piksel: glScissor icin gerekli (UI olcek %100 degilse fark eder)
    const std::optional<Affine2D> tBoxelToPixel = readBoxelToPixel();
    std::optional<ScissorGuard> tScissor;
    if (tBoxelToPixel)
    {
        tScissor.emplace(*tBoxelToPixel, toRectD(mArea));  // pencere disina tasma
    }

    const MapTheme& tTheme = mTheme;  // API katman renk yazmalari uygulanmis etkin tema
    Ui::fillRect(mCanvas, mArea, tTheme.mBackground);  // zemin verisi olmayan yer: duz renk

    int tMouseX = 0;
    int tMouseY = 0;
    const bool tMouseInside = mouseInside(&tMouseX, &tMouseY);

    drawContext tContext;
    tContext.mView = &mView;
    tContext.mCanvas = &mCanvas;
    tContext.mTheme = &tTheme;
    tContext.mArea = toRectD(mArea);
    tContext.mClearArea = clearArea();
    tContext.mPixelsPerBoxel = tBoxelToPixel ? pixelsPerBoxel(*tBoxelToPixel) : 1.0;
    if (mText)
    {
        mText->setPixelsPerBoxel(tContext.mPixelsPerBoxel);  // UI olcegi degistiyse atlas yeniden basilir
    }
    tContext.mNow = tNow;
    mLastPixelsPerBoxel = tContext.mPixelsPerBoxel;
    // Fare kapaliyken imlece bagli seyler (tooltip, cizim cizgisi) yok
    tContext.mHasCursor = mMouseInput && tMouseInside && !isOverControls(tMouseX, tMouseY);
    tContext.mCursorX = tMouseX;
    tContext.mCursorY = tMouseY;
    mLabelPlacer.reset(tContext.mClearArea);  // etiketler seritlerin altina girmesin
    tContext.mLabels = &mLabelPlacer;
    // 0.18: harita resmi onbellekte (XMapImageCache). Atlas yazisi gerekli: XPLMDrawString X-Plane'in
    // kendi matrisleriyle cizer, FBO'ya dogru cizecegi belli degil.
    const bool tNewData = mLayers.prepareAll();  // hazir dokular, vektorler
    bool tCached = false;
    if (mUseImageCache && mText && tBoxelToPixel)
    {
        const uint64_t tSignature = sceneSignature();
        const bool tUrgent = tSignature != mLastSignature;
        mLastSignature = tSignature;
        const bool tBusyEnded = mWasBusy && !mDataBusy;  // son gelen veri de resme girsin
        mWasBusy = mDataBusy;
        tCached = mImageCache.draw(mCanvas, mView, *tBoxelToPixel, tNow, tUrgent, tNewData || tBusyEnded, mDataBusy,
                                   [this, &tContext, &tTheme](const XMapView& pView, const RectD& pArea)
                                   {
                                       drawContext tImage = tContext;
                                       tImage.mView = &pView;
                                       tImage.mArea = pArea;
                                       tImage.mHasCursor = false;  // imlece bagli cizim resme girmez
                                       Gfx::Mesh tBackground;
                                       tBackground.addRect(pArea, tTheme.mBackground);
                                       mCanvas.draw(tBackground);
                                       mLabelPlacer.reset(tImage.mClearArea);
                                       mLayers.drawAll(tImage, LayerPass::Cached);
                                   });
        if (!tCached && !mImageCacheWarned)
        {
            log("map image cache (FBO) not available, drawing the map directly every frame");
            mImageCacheWarned = true;
        }
    }
    if (tCached)
    {
        mLabelPlacer.reset(tContext.mClearArea);
        mLayers.drawAll(tContext, LayerPass::Overlay);  // ucak, iz, yarim cizim: her karede ustte
    }
    else
    {
        mLayers.drawAll(tContext);
    }
    if (tContext.mHasCursor)
    {
        drawTooltip(tContext, mLayers.describeAt(tContext, tMouseX, tMouseY));
    }

    drawControls(tMouseX, tMouseY, tMouseInside);
    mCanvas.flushText();  // bekleyen yazilar (tek cizim cagrisi)

    // Bizim cizim suremiz (yumusatilmis): teshis modunda alt seritte gorunur
    const double tDrawMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tDrawStart).count();
    mDrawMs = mDrawMs * 0.9 + tDrawMs * 0.1;

    if (mDiagnostics && tNow - mLastDiagnosticsTime >= kDiagnosticsPeriod)
    {
        mLastDiagnosticsTime = tNow;
        logDiagnostics(tNow, tContext.mPixelsPerBoxel, tScissor ? tScissor->pixelRect() : RectD());
    }
}

void XMapWindow::drawTooltip(const drawContext& pContext, const std::string& pText)
{
    if (pText.empty())
    {
        return;
    }
    // Imlecin sag altina; pencereden tasacaksa soluna / ustune
    const int tPadding = 5;
    const int tWidth = Ui::textWidth(pText) + 2 * tPadding;
    const int tHeight = Ui::fontHeight() + 2 * tPadding;
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
    Ui::fillRect(mCanvas, tBox, Ui::kPanelBg);
    Ui::frameRect(mCanvas, tBox, Ui::kBorder);
    Ui::text(tLeft + tPadding, tBox.mBottom + tPadding + 1, pText, Ui::kTextWhite);
}

void XMapWindow::createTextRenderer()
{
    if (mXPlaneFont)
    {
        mText.reset();
        Ui::setTextRenderer(nullptr);
        log("text: X-Plane font (XPLMDrawString), [text] renderer = xplane");
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
    if (!tText->load(tFontPath, &tError))
    {
        log("text: own font atlas not available ({}), using X-Plane font", tError);
        return;
    }
    mText = std::move(tText);
    Ui::setTextRenderer(mText.get());
    log("text: font atlas from {} (cap height {} boxels)", tFontPath, tHeight);
}

bool XMapWindow::prepareCanvas()
{
    const bool tFirstTry = !mCanvasChecked;
    mCanvasChecked = true;
    if (mCanvas.init())
    {
        if (tFirstTry)
        {
            log("openGL ready: {}", mCanvas.contextText());
            createTextRenderer();
        }
        return true;
    }
    if (tFirstTry)
    {
        log("ERROR: map cannot draw: {}", mCanvas.errorText());
    }
    // Sekil cizemiyoruz ama X-Plane'in yazisi calisir: nedeni pencereye yaz
    Ui::text(mArea.mLeft + 10, mArea.mTop - 30, "XMap cannot draw: " + mCanvas.errorText(), Ui::kTextYellow);
    return false;
}

std::string XMapWindow::performanceText() const
{
    const float tPeriod = mFramePeriodRef ? XPLMGetDataf(mFramePeriodRef) : 0.0f;
    return std::format("FPS {:.0f}  draw {:.2f} ms  map renders {}/s", tPeriod > 0.0f ? 1.0f / tPeriod : 0.0f,
                       mDrawMs, mImageCache.rendersPerSecond());
}

std::string XMapWindow::hintText() const
{
    switch (mDrawings.getMode())
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

void XMapWindow::drawControls(int pMouseX, int pMouseY, bool pMouseInside)
{
    if (!mShowControls)
    {
        // Kontroller gizli: Standalone'da imlec haritanin ustundeyken kucuk bir
        // "Controls" dugmesi goster; imlec cikinca sadece harita kalir. Embedded'de hic bir sey.
        if (pMouseInside && !mEmbedded)
        {
            const RectI tButton = showControlsButtonRect();
            Ui::Button(mCanvas, tButton, "Controls", false, true, tButton.contains(pMouseX, pMouseY));
        }
        return;
    }

    const ToolbarState tState = currentToolbarState();
    mLayerPanel.layout(mToolbar.buttonRect(ToolbarAction::Layers), mArea.mBottom + StatusBar::kHeight + 4, mLayers);
    mToolPalette.layout(paletteBounds(), tState);
    mModePanel.layout(mToolbar.buttonRect(ToolbarAction::MapMode));
    mSearchPanel.layout(mToolbar.buttonRect(ToolbarAction::goToAirport));

    // Imlecin altindaki nokta: kontrollerin ustundeyse gosterme
    std::optional<LatLon> tCursor;
    if (pMouseInside && !isOverControls(pMouseX, pMouseY))
    {
        tCursor = mView.boxelToLatLon(pMouseX, pMouseY);
    }

    // Durum yazisi onceligi: yukleme mesaji > teshis (performans) > cizim ipucu
    std::string tMessage = mStatusMessage;
    if (tMessage.empty() && mDiagnostics)
    {
        tMessage = performanceText();
    }
    if (tMessage.empty())
    {
        tMessage = hintText();
    }

    mToolbar.draw(mCanvas, pMouseX, pMouseY, tState);
    mStatusBar.draw(mCanvas, mArea, mView, tCursor, tMessage);
    mToolPalette.draw(mCanvas, pMouseX, pMouseY, tState);
    mLayerPanel.draw(mCanvas, pMouseX, pMouseY, mLayers);  // en ustte: acilir paneller
    mModePanel.draw(mCanvas, pMouseX, pMouseY, mMapMode);
    mSearchPanel.draw(mCanvas, pMouseX, pMouseY, hasKeyboardFocus(), XPLMGetElapsedTime());
}

RectD XMapWindow::clearArea() const
{
    RectD tClear = toRectD(mArea);
    if (mShowControls)
    {
        tClear.mTop -= mToolbar.height();
        tClear.mBottom += StatusBar::kHeight;
    }
    return tClear;
}

RectI XMapWindow::paletteBounds() const
{
    RectI tBounds = mArea;
    tBounds.mTop -= mToolbar.height() + 4;
    tBounds.mBottom += StatusBar::kHeight + 4;
    tBounds.mLeft += 4;
    tBounds.mRight -= 4;
    return tBounds;
}

RectI XMapWindow::showControlsButtonRect() const
{
    RectI tRect;
    tRect.mLeft = mArea.mLeft + 4;
    tRect.mTop = mArea.mTop - 4;
    tRect.mRight = tRect.mLeft + Ui::buttonWidth("Controls");
    tRect.mBottom = tRect.mTop - (Toolbar::kRowHeight - 8);
    return tRect;
}

ToolbarState XMapWindow::currentToolbarState() const
{
    ToolbarState tState;
    tState.mLayersOpen = mLayerPanel.isOpen();
    tState.mModesOpen = mModePanel.isOpen();
    tState.mSearchOpen = mSearchPanel.isOpen();
    static const char* const kModeLabels[kMapModeCount] = {"XMap: Scenery", "XMap: VFR", "XMap: IFR Low",
                                                           "XMap: IFR High", "XMap: Dark"};
    tState.mModeLabel = kModeLabels[static_cast<int>(mMapMode)];
    tState.mFollowing = mFollowAircraft;
    tState.mHasAircraft = mAircraft.state().mValid;
    tState.mPoppedOut = mWindow && XPLMWindowIsPoppedOut(mWindow);
    tState.mToolsOpen = mToolPalette.isVisible();
    tState.mMode = mDrawings.getMode();
    tState.mCanFinish = mDrawings.current() != nullptr;
    tState.mCanUndo = tState.mCanFinish || mDrawings.shapeCount() > 0;
    return tState;
}

bool XMapWindow::mouseInside(int* pX, int* pY) const
{
    if (mWindow && !XPLMWindowIsPoppedOut(mWindow))
    {
        // X-Plane penceresinin icinde: imlecin yeri her an okunabilir
        XPLMGetMouseLocationGlobal(pX, pY);
        return mArea.contains(*pX, *pY);
    }
    // Disari alinmis pencere: cursor callback'ten gelen son konum
    *pX = mMouseX;
    *pY = mMouseY;
    return XPLMGetElapsedTime() - mMouseTime < kMouseTimeout && mArea.contains(mMouseX, mMouseY);
}

bool XMapWindow::isOverControls(int pX, int pY) const
{
    if (!mShowControls)
    {
        return false;
    }
    std::optional<MapMode> tMode;
    return mToolbar.contains(pX, pY) || mStatusBar.contains(pX, pY) || mLayerPanel.hitTest(pX, pY, mLayers).mInside ||
           mModePanel.hitTest(pX, pY, &tMode) || mToolPalette.contains(pX, pY) ||
           mSearchPanel.hitTest(pX, pY).mInside;
}

void XMapWindow::logDiagnostics(float pNow, double pPixelsPerBoxel, const RectD& pScissor)
{
    int tMouseX = 0;
    int tMouseY = 0;
    const bool tInside = mouseInside(&tMouseX, &tMouseY);
    int tGlobalX = 0;
    int tGlobalY = 0;
    XPLMGetMouseLocationGlobal(&tGlobalX, &tGlobalY);
    const LatLon tCenter = mView.center();
    log("DIAG t={:.1f} window=({},{},{},{}) popped_out={} pixels_per_boxel={:.3f} "
        "scissor=({:.0f},{:.0f},{:.0f},{:.0f}) "
        "zoom={:.2f} center={:.5f},{:.5f} mouse=({},{},{}) mouse_global=({},{}) aircraft={} labels={} {}",
        pNow, mArea.mLeft, mArea.mTop, mArea.mRight, mArea.mBottom, XPLMWindowIsPoppedOut(mWindow),
        pPixelsPerBoxel, pScissor.mLeft, pScissor.mTop, pScissor.mRight, pScissor.mBottom, mView.zoom(),
        tCenter.mLat, tCenter.mLon, tMouseX, tMouseY, tInside ? "inside" : "outside", tGlobalX, tGlobalY,
        mAircraft.state().mValid ? 1 : 0, mLabelPlacer.placedCount(), performanceText());
}

void XMapWindow::rememberMouse(int pX, int pY)
{
    mMouseX = pX;
    mMouseY = pY;
    mMouseTime = XPLMGetElapsedTime();
}

int XMapWindow::onClick(int pX, int pY, XPLMMouseStatus pStatus)
{
    rememberMouse(pX, pY);
    useOwnText();  // dugme genislikleri bu pencerenin yazisiyla olculur

    if (pStatus == xplm_MouseDown)
    {
        mPaletteDrag = false;
        mPassClick = false;
        const bool tOnControls = handleControlClick(pX, pY);
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
            setKeyboardFocus(false);  // haritaya tiklandi: tuslar yine X-Plane'e (panel acik kalir)
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
        mToolPalette.moveBy(pX - mDragLastX, pY - mDragLastY);
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
            mView.panByBoxels(pX - mDragLastX, pY - mDragLastY);
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
            handleMapClick(mPressX, mPressY);
        }
        if (mPaletteDrag)
        {
            saveSettings();  // paletin yeni yeri
        }
        mPressOnMap = false;
        mPaletteDrag = false;
    }
    return 1;  // tiklamayi her zaman tuket (gecirmek fare takibini bozuyor)
}

int XMapWindow::onRightClick(int pX, int pY, XPLMMouseStatus pStatus)
{
    rememberMouse(pX, pY);
    useOwnText();
    if (!mMouseInput)
    {
        return isOverControls(pX, pY) ? 1 : 0;  // fare kapali: haritadaki sag tik alttaki pencereye
    }
    // Sag tik: yarim kalan rota/bolgeyi bitir (tiklamayi her durumda tuket)
    if (pStatus == xplm_MouseDown && !isOverControls(pX, pY) && mDrawings.current())
    {
        mDrawings.finish();
        saveDrawings();
    }
    return 1;
}

void XMapWindow::handleMapClick(int pX, int pY)
{
    if (mDrawings.getMode() == Drawings::Mode::Pan)
    {
        return;
    }
    mDrawings.addVertex(mView.boxelToLatLon(pX, pY));
    if (mDrawings.getMode() == Drawings::Mode::Point)
    {
        saveDrawings();  // nokta hemen bitmis bir sekil
    }
}

int XMapWindow::onWheel(int pX, int pY, int pWheel, int pClicks)
{
    rememberMouse(pX, pY);
    useOwnText();
    if (pWheel != 0)
    {
        return 1;  // yatay tekerlek: kullanmiyoruz
    }
    if (mShowControls && mSearchPanel.hitTest(pX, pY).mInside)
    {
        return 1;  // arama panelinin ustunde zoom yok
    }
    if (mShowControls && mLayerPanel.scroll(pX, pY, pClicks))
    {
        return 1;  // uzun katman listesini kaydir (harita zoom yapmasin)
    }
    if (!mMouseInput)
    {
        return isOverControls(pX, pY) ? 1 : 0;  // fare kapali: tekerlek alttaki pencereye
    }
    const double tDelta = pClicks * mZoomStep;
    if (mFollowAircraft && mAircraft.state().mValid)
    {
        // Takipteyken merkez ucakta kalmali: ucagin etrafinda zoom
        mView.setZoom(mView.zoom() + tDelta);
    }
    else
    {
        // Imlecin altindaki nokta yerinde kalir
        mView.zoomAt(pX, pY, tDelta);
    }
    return 1;
}

bool XMapWindow::handleControlClick(int pX, int pY)
{
    if (!mShowControls)
    {
        if (!mEmbedded && showControlsButtonRect().contains(pX, pY))
        {
            toggleControls();
            return true;
        }
        return false;
    }

    // Acik paneller en ustte: once onlara bak
    const AirportSearchPanel::Hit tSearchHit = mSearchPanel.hitTest(pX, pY);
    if (tSearchHit.mInside)
    {
        if (tSearchHit.mAirport >= 0)
        {
            goToAirport(tSearchHit.mAirport);
        }
        else
        {
            setKeyboardFocus(true);  // panelin herhangi bir yerine tiklama: yazmaya devam
        }
        return true;
    }
    std::optional<MapMode> tModeHit;
    if (mModePanel.hitTest(pX, pY, &tModeHit))
    {
        if (tModeHit)
        {
            setMapMode(*tModeHit);
            mModePanel.setOpen(false);
        }
        return true;
    }
    const LayerPanel::Hit tPanelHit = mLayerPanel.hitTest(pX, pY, mLayers);
    if (tPanelHit.mInside)
    {
        applyLayerPanelHit(tPanelHit);
        return true;
    }
    const ToolPalette::Hit tPaletteHit = mToolPalette.hitTest(pX, pY, currentToolbarState());
    if (tPaletteHit.mInside)
    {
        if (tPaletteHit.mStartDrag)
        {
            mPaletteDrag = true;
        }
        else if (tPaletteHit.mToggleCollapse)
        {
            mToolPalette.setCollapsed(!mToolPalette.isCollapsed());
            saveSettings();
        }
        else
        {
            performToolbarAction(tPaletteHit.mAction);
        }
        return true;
    }
    if (mToolbar.contains(pX, pY))
    {
        performToolbarAction(mToolbar.hitTest(pX, pY, currentToolbarState()));
        return true;
    }
    return mStatusBar.contains(pX, pY);
}

void XMapWindow::applyLayerPanelHit(const LayerPanel::Hit& pHit)
{
    if (pHit.mResetDefaults)
    {
        resetLayers();
        return;
    }
    if (!pHit.mExpandKey.empty())
    {
        mLayerPanel.toggleExpanded(pHit.mExpandKey);  // grubu / katmani ac-kapa (gorunurluk degismez)
        return;
    }
    // Paneldeki satiri API ogesine cevir; kurallar engine/LayerTable'da (API ile ayni yol)
    std::optional<prime::MapLayer> tItem;
    if (pHit.mSubLayerOwner >= 0)
    {
        tItem = LayerRules::layerFromKey(LayerManager::subLayerKey(*mLayers.at(pHit.mSubLayerOwner), pHit.mSubLayer));
    }
    else if (!pHit.mToggle.empty())
    {
        tItem = LayerRules::layerFromKey(mLayers.at(pHit.mToggle.front())->id());
        if (tItem && pHit.mIsGroupBox)
        {
            tItem = LayerRules::parentOf(*tItem);
        }
    }
    if (!tItem)
    {
        return;
    }
    if (LayerRules::toggle(mLayers, themeFor(mMapMode), *tItem))
    {
        log("layer '{}': {}", LayerRules::layerKey(*tItem), LayerRules::isVisible(mLayers, *tItem) ? "on" : "off");
        saveSettings();
    }
}

void XMapWindow::resetLayers()
{
    mLayers.resetToDefaults(themeFor(mMapMode));
    log("layers reset to the defaults of {} mode", themeFor(mMapMode).mId);
    saveSettings();
}

void XMapWindow::setMapMode(MapMode pMode)
{
    if (pMode == mMapMode)
    {
        return;
    }
    mLayers.saveVisibility(mSettings, themeFor(mMapMode));  // eski modun katmanlari
    mMapMode = pMode;
    mTheme = themeFor(mMapMode);
    applyLayerColors();  // API renk yazmalari yeni temaya tasinir
    mLayers.loadVisibility(mSettings, themeFor(mMapMode));
    log("map mode: {}", themeFor(mMapMode).mId);
    saveSettings();
}

void XMapWindow::nextMapMode()
{
    setMapMode(nextMode(mMapMode));
}

GeoBounds XMapWindow::visibleBounds() const
{
    return mView.visibleBounds();
}

MapMode XMapWindow::getMapMode() const
{
    return mMapMode;
}

void XMapWindow::performToolbarAction(ToolbarAction pAction)
{
    switch (pAction)
    {
        case ToolbarAction::Layers:
            mLayerPanel.toggle();
            mModePanel.setOpen(false);
            setSearchOpen(false);
            break;
        case ToolbarAction::MapMode:
            mModePanel.toggle();
            mLayerPanel.setOpen(false);
            setSearchOpen(false);
            break;
        case ToolbarAction::goToAirport:
            setSearchOpen(!mSearchPanel.isOpen());
            mLayerPanel.setOpen(false);
            mModePanel.setOpen(false);
            break;
        case ToolbarAction::FollowAircraft:
            mFollowAircraft = !mFollowAircraft;
            if (mFollowAircraft)
            {
                centerOnAircraft();
            }
            break;
        case ToolbarAction::CenterAircraft:
            centerOnAircraft();
            break;
        case ToolbarAction::drawTools:
            mToolPalette.setVisible(!mToolPalette.isVisible());
            if (!mToolPalette.isVisible() && mDrawings.getMode() != Drawings::Mode::Pan)
            {
                mDrawings.setMode(Drawings::Mode::Pan);  // gizli palette cizim modunda kalinmasin
            }
            saveDrawings();
            saveSettings();
            break;
        case ToolbarAction::ModePan:
            mDrawings.setMode(Drawings::Mode::Pan);
            saveDrawings();  // mod degisince yarim cizim bitirilmis olabilir
            break;
        case ToolbarAction::ModePoint:
            mDrawings.setMode(Drawings::Mode::Point);
            saveDrawings();
            break;
        case ToolbarAction::ModeRoute:
            mDrawings.setMode(Drawings::Mode::Route);
            saveDrawings();
            break;
        case ToolbarAction::ModeRegion:
            mDrawings.setMode(Drawings::Mode::Region);
            saveDrawings();
            break;
        case ToolbarAction::Finish:
            mDrawings.finish();
            saveDrawings();
            break;
        case ToolbarAction::Undo:
            mDrawings.undo();
            saveDrawings();
            break;
        case ToolbarAction::Clear:
            mDrawings.clear();
            saveDrawings();
            break;
        case ToolbarAction::popOut:
            togglePopOut();
            break;
        case ToolbarAction::HideControls:
            toggleControls();
            break;
        case ToolbarAction::None:
            break;
    }
}

void XMapWindow::togglePopOut()
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
        log("window moved back into X-Plane");
    }
    else
    {
        XPLMSetWindowPositioningMode(mWindow, xplm_WindowPopOut, -1);
        log("window popped out into its own OS window");
    }
    mPressOnMap = false;
}

void XMapWindow::setSearchOpen(bool pOpen)
{
    mSearchPanel.setOpen(pOpen);
    setKeyboardFocus(pOpen);
}

void XMapWindow::setKeyboardFocus(bool pFocus)
{
    if (!mWindow)
    {
        return;
    }
    if (pFocus)
    {
        XPLMTakeKeyboardFocus(mWindow);
    }
    else if (hasKeyboardFocus())
    {
        XPLMTakeKeyboardFocus(nullptr);  // klavye X-Plane'e geri doner
    }
}

bool XMapWindow::hasKeyboardFocus() const
{
    return mWindow && XPLMHasKeyboardFocus(mWindow) != 0;
}

void XMapWindow::onKey(char pKey, XPLMKeyFlags pFlags, char pVirtualKey, bool pLosingFocus)
{
    if (pLosingFocus || !mSearchPanel.isOpen() || (pFlags & xplm_DownFlag) == 0)
    {
        return;  // odak gidiyor / panel kapali / tus birakildi
    }
    switch (static_cast<unsigned char>(pVirtualKey))
    {
        case XPLM_VK_ESCAPE:
            setSearchOpen(false);
            return;
        case XPLM_VK_RETURN:
        case XPLM_VK_ENTER:
        case XPLM_VK_NUMPAD_ENT:
            if (mSearchPanel.selectedAirport() >= 0)
            {
                goToAirport(mSearchPanel.selectedAirport());
            }
            return;
        case XPLM_VK_BACK:
            mSearchPanel.backspace();
            return;
        case XPLM_VK_UP:
            mSearchPanel.moveSelection(-1);
            return;
        case XPLM_VK_DOWN:
            mSearchPanel.moveSelection(1);
            return;
        default:
            break;
    }
    if ((pFlags & (xplm_ControlFlag | xplm_OptionAltFlag)) == 0)
    {
        mSearchPanel.typeCharacter(pKey);
    }
}

void XMapWindow::goToAirport(int pAirport)
{
    const AirportGeometry& tAirport = mAirports.at(pAirport);
    mFollowAircraft = false;  // takip acik kalirsa harita hemen ucaga geri doner
    mView.setCenter(airportCenter(tAirport));
    mView.setZoom(zoomToFitAirport(tAirport, mArea.width(), mArea.height()));
    log("go to airport {} ({})", tAirport.mId, tAirport.mName);
    setSearchOpen(false);
    saveSettings();
}

void XMapWindow::centerOnAircraft()
{
    if (mAircraft.state().mValid)
    {
        mView.setCenter(mAircraft.state().mPosition);
    }
}

void XMapWindow::applySettings()
{
    // Dosyada olmayan anahtar icin varsayilan = SU ANKI deger: pencere acilmadan once
    // API ile verilen ayarlar, dosyada karsiligi yoksa oldugu gibi kalir.
    const LatLon tCurrent = mView.center();
    // Once sinirlar: zoom yeni sinirlara gore kirpilsin
    mView.setZoomLimits(mSettings.getDouble(kViewSection, "min_zoom", mView.minZoomLimit()),
                        mSettings.getDouble(kViewSection, "max_zoom", mView.maxZoomLimit()));
    LatLon tCenter;
    tCenter.mLat = mSettings.getDouble(kViewSection, "center_lat", tCurrent.mLat);
    tCenter.mLon = mSettings.getDouble(kViewSection, "center_lon", tCurrent.mLon);
    mView.setCenter(tCenter);
    mView.setZoom(mSettings.getDouble(kViewSection, "zoom", mView.zoom()));
    mFollowAircraft = mSettings.getBool(kViewSection, "follow_aircraft", mFollowAircraft);
    mMapMode = modeFromId(mSettings.getString(kViewSection, "mode", themeFor(mMapMode).mId), mMapMode);
    mShowControls = mSettings.getBool(kViewSection, "show_controls", mShowControls);
    mMouseInput = mSettings.getBool(kViewSection, "mouse_input", mMouseInput);
    const double tZoomStep = mSettings.getDouble(kViewSection, "zoom_step", mZoomStep);
    if (tZoomStep > 0.0 && tZoomStep <= 5.0)
    {
        mZoomStep = tZoomStep;
    }
    const double tPanStep = mSettings.getDouble(kViewSection, "pan_step", mPanStep);
    if (tPanStep > 0.0 && tPanStep <= 1.0)
    {
        mPanStep = tPanStep;
    }

    mUseImageCache = mSettings.getBool(kRenderSection, "cache", mUseImageCache);
    setMaxRenderFps(mSettings.getInt(kRenderSection, "max_fps", mMaxRenderFps));
    setXPlaneFont(mSettings.getString(kTextSection, "renderer", mXPlaneFont ? "xplane" : "atlas") == "xplane");

    mToolPalette.setVisible(mSettings.getBool(kToolsSection, "visible", mToolPalette.isVisible()));
    mToolPalette.setCollapsed(mSettings.getBool(kToolsSection, "collapsed", mToolPalette.isCollapsed()));
    mToolPalette.setOffset(mSettings.getInt(kToolsSection, "offset_right", mToolPalette.offsetRight()),
                           mSettings.getInt(kToolsSection, "offset_top", mToolPalette.offsetTop()));

    mLayers.loadVisibility(mSettings, themeFor(mMapMode));
    mLayers.loadZoomRanges(mSettings);
    mImageCache.invalidate();
}

void XMapWindow::saveSettings()
{
    const LatLon tCenter = mView.center();
    mSettings.setDouble(kViewSection, "center_lat", tCenter.mLat);
    mSettings.setDouble(kViewSection, "center_lon", tCenter.mLon);
    mSettings.setDouble(kViewSection, "zoom", mView.zoom());
    mSettings.setBool(kViewSection, "follow_aircraft", mFollowAircraft);
    mSettings.setString(kViewSection, "mode", themeFor(mMapMode).mId);
    mSettings.setBool(kViewSection, "show_controls", mShowControls);
    mSettings.setBool(kViewSection, "mouse_input", mMouseInput);
    mSettings.setDouble(kViewSection, "min_zoom", mView.minZoomLimit());
    mSettings.setDouble(kViewSection, "max_zoom", mView.maxZoomLimit());
    mSettings.setDouble(kViewSection, "zoom_step", mZoomStep);
    mSettings.setDouble(kViewSection, "pan_step", mPanStep);

    mSettings.setBool(kRenderSection, "cache", mUseImageCache);
    mSettings.setInt(kRenderSection, "max_fps", mMaxRenderFps);
    mSettings.setString(kTextSection, "renderer", mXPlaneFont ? "xplane" : "atlas");

    mSettings.setBool(kToolsSection, "visible", mToolPalette.isVisible());
    mSettings.setBool(kToolsSection, "collapsed", mToolPalette.isCollapsed());
    mSettings.setInt(kToolsSection, "offset_right", mToolPalette.offsetRight());
    mSettings.setInt(kToolsSection, "offset_top", mToolPalette.offsetTop());

    mLayers.saveVisibility(mSettings, themeFor(mMapMode));
    mLayers.saveZoomRanges(mSettings);
    ++mSettingsRevision;  // degisti: XMapInstance ayar dosyasini yazar (host da bu sayaca bakabilir)
}

void XMapWindow::saveDrawings()
{
    ++mDrawingsRevision;  // host bunu gorup exportDrawings ile dosyaya yazar
}

std::string XMapWindow::exportSettings()
{
    saveSettings();
    return mSettings.toText({kViewSection, kToolsSection, "layers*", "layer_zoom", kRenderSection, kTextSection});
}

void XMapWindow::importSettings(const std::string& pText)
{
    // Once su anki durum bellege (API ile yapilip henuz yazilmamis degisiklikler kaybolmasin),
    // sonra metindeki anahtarlar ustune; metinde olmayan anahtar oldugu gibi kalir.
    // Eski map_settings.ini aynen okunur (bilinmeyen bolumler zararsiz).
    saveSettings();
    mSettings.mergeText(pText);
    applySettings();
    ++mSettingsRevision;
}

void XMapWindow::applyLayerColors()
{
    // Yazma yok: tema modun kendi renkleriyle kalir. Varsa dogrudan tema rengi olan
    // katmanlara yazilir (tablo eslesmeyen katmanlar yoksayilir).
    for (const auto& [tLayer, tColor] : mLayerColors)
    {
        if (Rgba* tSlot = themeLayerColor(mTheme, tLayer))
        {
            *tSlot = tColor;
        }
    }
}

bool XMapWindow::setLayerColor(prime::MapLayer pLayer, const Rgba& pColor)
{
    if (!themeLayerColor(mTheme, pLayer))
    {
        return false;  // bu katmanin dogrudan tema rengi yok
    }
    mLayerColors[pLayer] = pColor;
    applyLayerColors();
    return true;
}

Rgba XMapWindow::layerColor(prime::MapLayer pLayer) const
{
    const auto tIt = mLayerColors.find(pLayer);
    if (tIt == mLayerColors.end())
    {
        return {-1.0f, 0.0f, 0.0f, 0.0f};  // yazma yok
    }
    return tIt->second;
}

void XMapWindow::clearLayerColor(prime::MapLayer pLayer)
{
    mLayerColors.erase(pLayer);
    mTheme = themeFor(mMapMode);
    applyLayerColors();
}

void XMapWindow::clearLayerColors()
{
    mLayerColors.clear();
    mTheme = themeFor(mMapMode);
    applyLayerColors();
}

void XMapWindow::setLayerColors(const std::map<prime::MapLayer, Rgba>& pColors)
{
    mLayerColors = pColors;
    mTheme = themeFor(mMapMode);
    applyLayerColors();
}

const std::map<prime::MapLayer, Rgba>& XMapWindow::layerColors() const
{
    return mLayerColors;
}

void XMapWindow::render()
{
    onDraw();
}

XMapView& XMapWindow::view()
{
    return mView;
}

const XMapView& XMapWindow::view() const
{
    return mView;
}

void XMapWindow::setGeometry(const RectI& pGeometry)
{
    mFloatingGeometry = pGeometry;
    mHasFloatingGeometry = true;
    if (mWindow && !XPLMWindowIsPoppedOut(mWindow))
    {
        XPLMSetWindowGeometry(mWindow, pGeometry.mLeft, pGeometry.mTop, pGeometry.mRight, pGeometry.mBottom);
    }
    mArea = pGeometry;
    mView.setViewport(toRectD(mArea));
}

RectI XMapWindow::getGeometry() const
{
    if (!mWindow)
    {
        return mFloatingGeometry;
    }
    RectI tGeometry;
    XPLMGetWindowGeometry(mWindow, &tGeometry.mLeft, &tGeometry.mTop, &tGeometry.mRight, &tGeometry.mBottom);
    return tGeometry;
}

void XMapWindow::bringToFront()
{
    if (mWindow)
    {
        XPLMBringWindowToFront(mWindow);
    }
}

void XMapWindow::setPoppedOut(bool pPoppedOut)
{
    if (mWindow && !mEmbedded && isPoppedOut() != pPoppedOut)
    {
        togglePopOut();
    }
}

bool XMapWindow::isPoppedOut() const
{
    return mWindow && XPLMWindowIsPoppedOut(mWindow) != 0;
}

void XMapWindow::setUiVisible(bool pVisible)
{
    if (mShowControls != pVisible)
    {
        toggleControls();
    }
}

bool XMapWindow::isUiVisible() const
{
    return mShowControls;
}

void XMapWindow::setMouseInput(bool pEnabled)
{
    mMouseInput = pEnabled;
    mPressOnMap = false;
    ++mSettingsRevision;
}

bool XMapWindow::isMouseInputEnabled() const
{
    return mMouseInput;
}

void XMapWindow::setFollowAircraft(bool pFollow)
{
    mFollowAircraft = pFollow;
    if (pFollow)
    {
        centerOnAircraft();
    }
    saveSettings();
}

bool XMapWindow::isFollowingAircraft() const
{
    return mFollowAircraft;
}

bool XMapWindow::isDiagnosticsOn() const
{
    return mDiagnostics;
}

void XMapWindow::drawingsChanged()
{
    saveDrawings();
}

std::vector<std::pair<std::string, std::string>> XMapWindow::describeAt(int pX, int pY)
{
    useOwnText();
    mLayers.setCurrentZoom(mView.zoom());
    drawContext tContext;
    tContext.mView = &mView;
    tContext.mCanvas = &mCanvas;
    tContext.mTheme = &mTheme;
    tContext.mArea = toRectD(mArea);
    tContext.mClearArea = clearArea();
    tContext.mPixelsPerBoxel = mLastPixelsPerBoxel;
    tContext.mHasCursor = true;
    tContext.mCursorX = pX;
    tContext.mCursorY = pY;
    tContext.mLabels = &mLabelPlacer;
    return mLayers.describeAllAt(tContext, pX, pY);
}

uint64_t XMapWindow::settingsRevision() const
{
    return mSettingsRevision;
}

uint64_t XMapWindow::drawingsRevision() const
{
    return mDrawingsRevision;
}
