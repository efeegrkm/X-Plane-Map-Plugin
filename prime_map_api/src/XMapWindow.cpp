#include "XMapWindow.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <format>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <optional>

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
const char* kFontFile = "Resources/fonts/DejaVuSans.ttf";

const int kMinWidth = 400;
const int kMinHeight = 260;
const int kMaxSize = 8192;
const int kEmbeddedMinSize = 50;
const float kMouseTimeout = 0.25f;
const float kDiagnosticsPeriod = 2.0f;
const int kClickSlop = 4;

const LatLon kDefaultCenter = {39.0, 35.0};
const double kDefaultZoom = 6.0;
}

XMapWindow::XMapWindow(LayerManager& pLayers, AircraftData& pAircraft, Drawings& pDrawings,
                     const AirportGeometryStore& pAirports)
    : mLayers(pLayers)
    , mAircraft(pAircraft)
    , mDrawings(pDrawings)
    , mAirports(pAirports)
    , mSearchPanel(pAirports)
{
    mView.setCenter(kDefaultCenter);
    mView.setZoom(kDefaultZoom);
    mLayers.resetToDefaults(themeFor(mMapMode));
}

XMapWindow::~XMapWindow()
{
    destroy();
}

void XMapWindow::create()
{
    if (mWindow || mTextureOpen)
    {
        return;
    }
    if (mTextureMode)
    {
        mTextureOpen = true;
        mArea = textureArea();
        mView.setViewport(toRectD(mArea));
        mTextureDirty = true;
        log("map texture created ({} x {} boxels)", mTextureWidth, mTextureHeight);
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
    tParams.drawWindowFunc = drawCallback;
    tParams.handleMouseClickFunc = clickCallback;
    tParams.handleRightClickFunc = rightClickCallback;
    tParams.handleKeyFunc = keyCallback;
    tParams.handleCursorFunc = cursorCallback;
    tParams.handleMouseWheelFunc = wheelCallback;
    tParams.refcon = this;
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
    std::string tTitle = mTitle;
    XPLMSetWindowTitle(mWindow, tTitle.data());
    mFramePeriodRef = XPLMFindDataRef("sim/operation/misc/frame_rate_period");
    log("map window created ({}, {},{} - {},{})", mEmbedded ? "embedded" : "standalone", mFloatingGeometry.mLeft,
        mFloatingGeometry.mTop, mFloatingGeometry.mRight, mFloatingGeometry.mBottom);

    if (mWantVisible)
    {
        setVisible(true);
    }
}

void XMapWindow::useOwnText()
{
    Ui::setTextRenderer(mText.get());
}

bool XMapWindow::isEmbedded() const
{
    return mEmbedded;
}

void XMapWindow::setEmbedded(bool pEmbedded)
{
    if (isCreated())
    {
        if (pEmbedded != mEmbedded)
        {
            log("WARNING: window style cannot change while the map is open (close it first)");
        }
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
    return mWindow != nullptr || mTextureOpen;
}

void XMapWindow::destroy()
{
    if (mWindow)
    {
        mWantVisible = XPLMGetWindowIsVisible(mWindow) != 0;
        if (hasKeyboardFocus())
        {
            XPLMTakeKeyboardFocus(nullptr);
        }
        mSearchPanel.setOpen(false);
        XPLMDestroyWindow(mWindow);
        mWindow = nullptr;
        mPressOnMap = false;
    }
    if (mTextureOpen)
    {
        mTextureOpen = false;
        mTextureKeyboard = false;
        mSearchPanel.setOpen(false);
        mPressOnMap = false;
    }
    if (mText && Ui::currentTextRenderer() == mText.get())
    {
        Ui::setTextRenderer(nullptr);
    }
}

bool XMapWindow::isVisible() const
{
    if (mTextureMode)
    {
        return mTextureOpen && mWantVisible;
    }
    return mWindow && XPLMGetWindowIsVisible(mWindow) != 0;
}

void XMapWindow::setVisible(bool pVisible)
{
    mWantVisible = pVisible;
    if (!mWindow)
    {
        ++mSettingsRevision;
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
        setSearchOpen(false);
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
    for (const auto& [tLayer, tColor] : mLayerColors)
    {
        tMix(static_cast<uint64_t>(static_cast<int>(tLayer)));
        tMix(static_cast<uint64_t>(static_cast<int>(tColor.mR * 65535.0f)) << 32 |
             static_cast<uint64_t>(static_cast<int>(tColor.mG * 65535.0f)) << 16 |
             static_cast<uint64_t>(static_cast<int>(tColor.mB * 65535.0f)));
    }
    tMix(static_cast<uint64_t>(mDrawings.shapeCount()));
    tMix(mDrawings.current() ? mDrawings.current()->mPoints.size() + 1 : 0);
    const RectD tClear = clearArea();
    tMix(static_cast<uint64_t>(tClear.mTop * 16.0));
    tMix(static_cast<uint64_t>(tClear.mBottom * 16.0));
    return tHash;
}

void XMapWindow::setStatusMessage(const std::string& pMessage)
{
    mStatusMessage = pMessage;
}

void XMapWindow::update(float )
{
    const bool tVisible = isVisible();
    if (mWasVisible && !tVisible)
    {
        mPressOnMap = false;
        saveSettings();
    }
    mWasVisible = tVisible;
    if (!tVisible && mSearchPanel.isOpen())
    {
        setSearchOpen(false);
    }
    mSearchPanel.refresh();
}

void XMapWindow::drawCallback(XPLMWindowID , void* pRefcon)
{
    static_cast<XMapWindow*>(pRefcon)->onDraw();
}

int XMapWindow::clickCallback(XPLMWindowID , int pX, int pY, XPLMMouseStatus pStatus, void* pRefcon)
{
    return static_cast<XMapWindow*>(pRefcon)->onClick(pX, pY, pStatus);
}

int XMapWindow::rightClickCallback(XPLMWindowID , int pX, int pY, XPLMMouseStatus pStatus,
                                  void* pRefcon)
{
    return static_cast<XMapWindow*>(pRefcon)->onRightClick(pX, pY, pStatus);
}

void XMapWindow::keyCallback(XPLMWindowID , char pKey, XPLMKeyFlags pFlags, char pVirtualKey,
                            void* pRefcon, int pLosingFocus)
{
    static_cast<XMapWindow*>(pRefcon)->onKey(pKey, pFlags, pVirtualKey, pLosingFocus != 0);
}

XPLMCursorStatus XMapWindow::cursorCallback(XPLMWindowID , int pX, int pY, void* pRefcon)
{
    static_cast<XMapWindow*>(pRefcon)->rememberMouse(pX, pY);
    return xplm_CursorArrow;
}

int XMapWindow::wheelCallback(XPLMWindowID , int pX, int pY, int pWheel, int pClicks, void* pRefcon)
{
    return static_cast<XMapWindow*>(pRefcon)->onWheel(pX, pY, pWheel, pClicks);
}

void XMapWindow::onDraw()
{
    useOwnText();

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
    prepareFrame();
    if (!beginCanvasFrame())
    {
        return;
    }

    const std::optional<Affine2D> tBoxelToPixel = readBoxelToPixel();
    std::optional<ScissorGuard> tScissor;
    if (tBoxelToPixel)
    {
        tScissor.emplace(*tBoxelToPixel, toRectD(mArea));
    }
    drawFrame(XPLMGetElapsedTime(), tBoxelToPixel, true, tScissor ? tScissor->pixelRect() : RectD());
}

void XMapWindow::prepareFrame()
{
    mView.setViewport(toRectD(mArea));

    mAircraft.refresh();
    if (mFollowAircraft && mAircraft.state().mValid)
    {
        mView.setCenter(mAircraft.state().mPosition);
    }

    if (mShowControls)
    {
        mToolbar.layout(mArea, currentToolbarState());
    }
}

bool XMapWindow::beginCanvasFrame()
{
    if (!prepareCanvas())
    {
        return false;
    }
    if (mTextPending)
    {
        mTextPending = false;
        createTextRenderer();
    }
    mCanvas.beginFrame();
    if (mCanvas.isContextUsable())
    {
        return true;
    }
    if (!mContextWarned)
    {
        log("ERROR: this openGL context does not share the map's shaders (popped out = {}, texture = {})",
            isPoppedOut(), mTextureMode);
        mContextWarned = true;
    }
    if (mWindow)
    {
        Ui::text(mArea.mLeft + 10, mArea.mTop - 30, "XMap cannot draw in this window, use Pop in", Ui::kTextYellow);
        mCanvas.flushText();
    }
    return false;
}

void XMapWindow::drawFrame(float pNow, const std::optional<Affine2D>& pBoxelToPixel, bool pUseImageCache,
                          const RectD& pScissorPixels)
{
    const std::chrono::steady_clock::time_point tDrawStart = std::chrono::steady_clock::now();
    const float tNow = pNow;

    const XMapTheme& tTheme = mTheme;
    Ui::fillRect(mCanvas, mArea, tTheme.mBackground);

    int tMouseX = 0;
    int tMouseY = 0;
    const bool tMouseInside = mouseInside(&tMouseX, &tMouseY);

    drawContext tContext;
    tContext.mView = &mView;
    tContext.mCanvas = &mCanvas;
    tContext.mTheme = &tTheme;
    tContext.mArea = toRectD(mArea);
    tContext.mClearArea = clearArea();
    tContext.mPixelsPerBoxel = pBoxelToPixel ? pixelsPerBoxel(*pBoxelToPixel) : 1.0;
    if (mText)
    {
        mText->setPixelsPerBoxel(tContext.mPixelsPerBoxel);
    }
    tContext.mNow = tNow;
    mLastPixelsPerBoxel = tContext.mPixelsPerBoxel;
    tContext.mHasCursor = mMouseInput && tMouseInside && !isOverControls(tMouseX, tMouseY);
    tContext.mCursorX = tMouseX;
    tContext.mCursorY = tMouseY;
    mLabelPlacer.reset(tContext.mClearArea);
    tContext.mLabels = &mLabelPlacer;
    const bool tNewData = mLayers.prepareAll();
    bool tCached = false;
    if (pUseImageCache && mUseImageCache && mText && pBoxelToPixel)
    {
        const uint64_t tSignature = sceneSignature();
        const bool tUrgent = tSignature != mLastSignature;
        mLastSignature = tSignature;
        const bool tBusyEnded = mWasBusy && !mDataBusy;
        mWasBusy = mDataBusy;
        tCached = mImageCache.draw(mCanvas, mView, *pBoxelToPixel, tNow, tUrgent, tNewData || tBusyEnded, mDataBusy,
                                   [this, &tContext, &tTheme](const XMapView& pView, const RectD& pArea)
                                   {
                                       drawContext tImage = tContext;
                                       tImage.mView = &pView;
                                       tImage.mArea = pArea;
                                       tImage.mHasCursor = false;
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
        mLayers.drawAll(tContext, LayerPass::Overlay);
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
    mCanvas.flushText();

    const double tDrawMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tDrawStart).count();
    mDrawMs = mDrawMs * 0.9 + tDrawMs * 0.1;

    if (mDiagnostics && tNow - mLastDiagnosticsTime >= kDiagnosticsPeriod)
    {
        mLastDiagnosticsTime = tNow;
        logDiagnostics(tNow, tContext.mPixelsPerBoxel, pScissorPixels);
    }
}

RectI XMapWindow::textureArea() const
{
    RectI tArea;
    tArea.mLeft = 0;
    tArea.mBottom = 0;
    tArea.mRight = mTextureWidth;
    tArea.mTop = mTextureHeight;
    return tArea;
}

void XMapWindow::setTextureMode(bool pTexture)
{
    if (isCreated())
    {
        if (pTexture != mTextureMode)
        {
            log("WARNING: window style cannot change while the map is open (close it first)");
        }
        return;
    }
    mTextureMode = pTexture;
}

bool XMapWindow::isTextureMode() const
{
    return mTextureMode;
}

void XMapWindow::setTextureSize(int pWidth, int pHeight, double pPixelsPerBoxel)
{
    const int tWidth = std::clamp(pWidth, 1, kMaxSize);
    const int tHeight = std::clamp(pHeight, 1, kMaxSize);
    if (tWidth == mTextureWidth && tHeight == mTextureHeight && pPixelsPerBoxel == mTexturePixelsPerBoxel)
    {
        return;
    }
    mTextureWidth = tWidth;
    mTextureHeight = tHeight;
    mTexturePixelsPerBoxel = pPixelsPerBoxel;
    mTextureDirty = true;
    if (mTextureMode)
    {
        mArea = textureArea();
        mView.setViewport(toRectD(mArea));
    }
}

int XMapWindow::textureWidth() const
{
    return mTextureWidth;
}

int XMapWindow::textureHeight() const
{
    return mTextureHeight;
}

bool XMapWindow::renderTexture()
{
    if (!mTextureMode || !mTextureOpen || !mWantVisible)
    {
        return false;
    }
    useOwnText();
    const float tNow = XPLMGetElapsedTime();
    mArea = textureArea();
    prepareFrame();

    double tScale = mTexturePixelsPerBoxel;
    if (tScale <= 0.0)
    {
        const std::optional<Affine2D> tHost = readBoxelToPixel();
        tScale = tHost ? pixelsPerBoxel(*tHost) : 1.0;
    }
    const int tPixelWidth = std::clamp(static_cast<int>(std::lround(mTextureWidth * tScale)), 1, kMaxSize);
    const int tPixelHeight = std::clamp(static_cast<int>(std::lround(mTextureHeight * tScale)), 1, kMaxSize);

    const uint64_t tSignature = sceneSignature();
    const bool tSceneChanged = tSignature != mLastSignature;
    const bool tViewChanged = mView.centerMercatorX() != mTextureCenterX ||
                              mView.centerMercatorY() != mTextureCenterY || mView.zoom() != mTextureZoom;
    const bool tSizeChanged = tPixelWidth != mTextureTarget.width() || tPixelHeight != mTextureTarget.height();
    const bool tRateAllows = tNow - mLastTextureRender >= 1.0f / static_cast<float>(mMaxRenderFps);
    const bool tHasImage = mTextureTarget.getTexture().isValid();
    if (tHasImage && !tSizeChanged && !tSceneChanged && !tViewChanged && !mTextureDirty && !tRateAllows)
    {
        return true;
    }

    if (!beginCanvasFrame())
    {
        return false;
    }
    if (!mText && !mTextureFontWarned)
    {
        log("WARNING: texture map uses the X-Plane font; labels may be misplaced ([text] renderer = atlas)");
        mTextureFontWarned = true;
    }
    if (mTextureTarget.prepare(tPixelWidth, tPixelHeight) == 0 ||
        !mCanvas.beginOffscreen(mTextureTarget, toRectD(mArea)))
    {
        return false;
    }
    Affine2D tBoxelToPixel;
    tBoxelToPixel.mA = static_cast<double>(tPixelWidth) / mTextureWidth;
    tBoxelToPixel.mD = static_cast<double>(tPixelHeight) / mTextureHeight;
    drawFrame(tNow, tBoxelToPixel, false, RectD());
    mCanvas.endOffscreen();

    mLastSignature = tSignature;
    mTextureCenterX = mView.centerMercatorX();
    mTextureCenterY = mView.centerMercatorY();
    mTextureZoom = mView.zoom();
    mTextureDirty = false;
    mLastTextureRender = tNow;
    return true;
}

unsigned int XMapWindow::textureId() const
{
    const Gfx::Texture& tTexture = mTextureTarget.getTexture();
    return tTexture.isValid() ? static_cast<unsigned int>(tTexture.id()) : 0u;
}

int XMapWindow::injectMouse(int pX, int pY, XPLMMouseStatus pStatus, bool pRight)
{
    if (!mTextureMode || !mTextureOpen)
    {
        return 0;
    }
    mTextureDirty = true;
    return pRight ? onRightClick(pX, pY, pStatus) : onClick(pX, pY, pStatus);
}

void XMapWindow::injectMouseMove(int pX, int pY)
{
    if (!mTextureMode || !mTextureOpen)
    {
        return;
    }
    if (pX != mMouseX || pY != mMouseY)
    {
        mTextureDirty = true;
    }
    rememberMouse(pX, pY);
}

int XMapWindow::injectWheel(int pX, int pY, int pClicks)
{
    if (!mTextureMode || !mTextureOpen)
    {
        return 0;
    }
    mTextureDirty = true;
    return onWheel(pX, pY, 0, pClicks);
}

bool XMapWindow::injectKey(char pKey, XPLMKeyFlags pFlags, char pVirtualKey)
{
    if (!wantsKeyboard())
    {
        return false;
    }
    mTextureDirty = true;
    onKey(pKey, pFlags, pVirtualKey, false);
    return true;
}

bool XMapWindow::wantsKeyboard() const
{
    return mTextureMode && mTextureOpen && mTextureKeyboard;
}

void XMapWindow::drawTooltip(const drawContext& pContext, const std::string& pText)
{
    if (pText.empty())
    {
        return;
    }
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
    if (mWindow)
    {
        Ui::text(mArea.mLeft + 10, mArea.mTop - 30, "XMap cannot draw: " + mCanvas.errorText(), Ui::kTextYellow);
    }
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
    mModePanel.layout(mToolbar.buttonRect(ToolbarAction::XMapMode));
    mSearchPanel.layout(mToolbar.buttonRect(ToolbarAction::goToAirport));

    std::optional<LatLon> tCursor;
    if (pMouseInside && !isOverControls(pMouseX, pMouseY))
    {
        tCursor = mView.boxelToLatLon(pMouseX, pMouseY);
    }

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
    mLayerPanel.draw(mCanvas, pMouseX, pMouseY, mLayers);
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
    static const char* const kModeLabels[kXMapModeCount] = {"XMap: Scenery", "XMap: VFR", "XMap: IFR Low",
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
        XPLMGetMouseLocationGlobal(pX, pY);
        return mArea.contains(*pX, *pY);
    }
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
    std::optional<XMapMode> tMode;
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
        pNow, mArea.mLeft, mArea.mTop, mArea.mRight, mArea.mBottom, isPoppedOut(),
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
    useOwnText();

    if (pStatus == xplm_MouseDown)
    {
        mPaletteDrag = false;
        mPassClick = false;
        const bool tOnControls = handleControlClick(pX, pY);
        if (!tOnControls && !mMouseInput)
        {
            mPassClick = true;
            mPressOnMap = false;
            return 0;
        }
        bringToFront();
        mPressOnMap = !tOnControls;
        if (mPressOnMap)
        {
            setKeyboardFocus(false);
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
        if (!mPressMoved && (std::abs(pX - mPressX) > kClickSlop || std::abs(pY - mPressY) > kClickSlop))
        {
            mPressMoved = true;
        }
        if (mPressMoved && (pX != mDragLastX || pY != mDragLastY))
        {
            mView.panByBoxels(pX - mDragLastX, pY - mDragLastY);
            mFollowAircraft = false;
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
            saveSettings();
        }
        mPressOnMap = false;
        mPaletteDrag = false;
    }
    return 1;
}

int XMapWindow::onRightClick(int pX, int pY, XPLMMouseStatus pStatus)
{
    rememberMouse(pX, pY);
    useOwnText();
    if (!mMouseInput)
    {
        return isOverControls(pX, pY) ? 1 : 0;
    }
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
        saveDrawings();
    }
}

int XMapWindow::onWheel(int pX, int pY, int pWheel, int pClicks)
{
    rememberMouse(pX, pY);
    useOwnText();
    if (pWheel != 0)
    {
        return 1;
    }
    if (mShowControls && mSearchPanel.hitTest(pX, pY).mInside)
    {
        return 1;
    }
    if (mShowControls && mLayerPanel.scroll(pX, pY, pClicks))
    {
        return 1;
    }
    if (!mMouseInput)
    {
        return isOverControls(pX, pY) ? 1 : 0;
    }
    const double tDelta = pClicks * mZoomStep;
    if (mFollowAircraft && mAircraft.state().mValid)
    {
        mView.setZoom(mView.zoom() + tDelta);
    }
    else
    {
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

    const AirportSearchPanel::Hit tSearchHit = mSearchPanel.hitTest(pX, pY);
    if (tSearchHit.mInside)
    {
        if (tSearchHit.mAirport >= 0)
        {
            goToAirport(tSearchHit.mAirport);
        }
        else
        {
            setKeyboardFocus(true);
        }
        return true;
    }
    std::optional<XMapMode> tModeHit;
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
        mLayerPanel.toggleExpanded(pHit.mExpandKey);
        return;
    }
    std::optional<prime::XMapLayer> tItem;
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

void XMapWindow::setMapMode(XMapMode pMode)
{
    if (pMode == mMapMode)
    {
        return;
    }
    mLayers.saveVisibility(mSettings, themeFor(mMapMode));
    mMapMode = pMode;
    mTheme = themeFor(mMapMode);
    applyLayerColors();
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

XMapMode XMapWindow::getMapMode() const
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
        case ToolbarAction::XMapMode:
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
                mDrawings.setMode(Drawings::Mode::Pan);
            }
            saveDrawings();
            saveSettings();
            break;
        case ToolbarAction::ModePan:
            mDrawings.setMode(Drawings::Mode::Pan);
            saveDrawings();
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
    if (!mWindow)
    {
        return;
    }
    if (XPLMWindowIsPoppedOut(mWindow))
    {
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
    if (mTextureMode)
    {
        mTextureKeyboard = pFocus && mTextureOpen;
        return;
    }
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
        XPLMTakeKeyboardFocus(nullptr);
    }
}

bool XMapWindow::hasKeyboardFocus() const
{
    if (mTextureMode)
    {
        return mTextureKeyboard;
    }
    return mWindow && XPLMHasKeyboardFocus(mWindow) != 0;
}

void XMapWindow::onKey(char pKey, XPLMKeyFlags pFlags, char pVirtualKey, bool pLosingFocus)
{
    if (pLosingFocus || !mSearchPanel.isOpen() || (pFlags & xplm_DownFlag) == 0)
    {
        return;
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
    mFollowAircraft = false;
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
    const LatLon tCurrent = mView.center();
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
    ++mSettingsRevision;
}

void XMapWindow::saveDrawings()
{
    ++mDrawingsRevision;
}

std::string XMapWindow::exportSettings()
{
    saveSettings();
    return mSettings.toText({kViewSection, kToolsSection, "layers*", "layer_zoom", kRenderSection, kTextSection});
}

void XMapWindow::importSettings(const std::string& pText)
{
    saveSettings();
    mSettings.mergeText(pText);
    applySettings();
    ++mSettingsRevision;
}

void XMapWindow::applyLayerColors()
{
    for (const auto& [tLayer, tColor] : mLayerColors)
    {
        if (Rgba* tSlot = themeLayerColor(mTheme, tLayer))
        {
            *tSlot = tColor;
        }
    }
}

bool XMapWindow::setLayerColor(prime::XMapLayer pLayer, const Rgba& pColor)
{
    if (!themeLayerColor(mTheme, pLayer))
    {
        return false;
    }
    mLayerColors[pLayer] = pColor;
    applyLayerColors();
    return true;
}

Rgba XMapWindow::layerColor(prime::XMapLayer pLayer) const
{
    const auto tIt = mLayerColors.find(pLayer);
    if (tIt == mLayerColors.end())
    {
        return {-1.0f, 0.0f, 0.0f, 0.0f};
    }
    return tIt->second;
}

void XMapWindow::clearLayerColor(prime::XMapLayer pLayer)
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

void XMapWindow::setLayerColors(const std::map<prime::XMapLayer, Rgba>& pColors)
{
    mLayerColors = pColors;
    mTheme = themeFor(mMapMode);
    applyLayerColors();
}

const std::map<prime::XMapLayer, Rgba>& XMapWindow::layerColors() const
{
    return mLayerColors;
}

void XMapWindow::render()
{
    if (mTextureMode)
    {
        renderTexture();
    }
    else if (mWindow)
    {
        onDraw();
    }
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
    if (mTextureMode)
    {
        return;
    }
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
    if (mTextureMode)
    {
        return textureArea();
    }
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
