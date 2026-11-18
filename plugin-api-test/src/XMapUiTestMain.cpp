#include <array>
#include <cstring>
#include <memory>
#include <string>

#include "XPLMDefs.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#include <GL/gl.h>

#include "XMapApiCalls.h"
#include "MultiXMapSelfTest.h"

namespace
{
std::string findPluginDirectory()
{
    std::array<char, 2048> tXplPath = {};
    XPLMGetPluginInfo(XPLMGetMyID(), nullptr, tXplPath.data(), nullptr, nullptr);
    std::string tPath = tXplPath.data();
    for (int tLevel = 0; tLevel < 2; ++tLevel)
    {
        const size_t tCut = tPath.find_last_of("/\\");
        if (tCut != std::string::npos)
        {
            tPath.erase(tCut);
        }
    }
    return tPath + XPLMGetDirectorySeparator();
}

enum class Button
{
    panUp,
    panDown,
    panLeft,
    panRight,
    zoomIn,
    zoomOut,
    Count,
};

constexpr int kButtonCount = static_cast<int>(Button::Count);

constexpr int kMargin = 14;
constexpr int kSideWidth = 54;
constexpr int kTopHeight = 46;
constexpr int kBottomHeight = 72;
constexpr int kButtonHalfW = 23;
constexpr int kButtonHalfH = 14;
constexpr double kPressFlash = 0.3;
constexpr double kStartLat = 47.45;
constexpr double kStartLon = -122.30;
constexpr double kStartZoom = 11.0;

constexpr const char* kButtonLabels[kButtonCount] = {
    "^", "v", "<", ">", "+", "-",
};
constexpr const char* kButtonNames[kButtonCount] = {
    "Pan up", "Pan down", "Pan left", "Pan right", "Zoom in", "Zoom out",
};

struct Rect
{
    int mLeft = 0;
    int mTop = 0;
    int mRight = 0;
    int mBottom = 0;
    bool contains(int pX, int pY) const
    {
        return pX >= mLeft && pX < mRight && pY >= mBottom && pY < mTop;
    }
    int width() const
    {
        return mRight - mLeft;
    }
    int height() const
    {
        return mTop - mBottom;
    }
};

class UiPlugin
{
public:
    void enable();
    void disable();

private:
    static void drawCallback(XPLMWindowID pWindow, void* pRefcon);
    static int clickCallback(XPLMWindowID pWindow, int pX, int pY, XPLMMouseStatus pStatus, void* pRefcon);
    static int rightClickCallback(XPLMWindowID pWindow, int pX, int pY, XPLMMouseStatus pStatus, void* pRefcon);
    static void keyCallback(XPLMWindowID pWindow, char pKey, XPLMKeyFlags pFlags, char pVirtualKey, void* pRefcon,
                            int pLosingFocus);
    static XPLMCursorStatus cursorCallback(XPLMWindowID pWindow, int pX, int pY, void* pRefcon);
    static int wheelCallback(XPLMWindowID pWindow, int pX, int pY, int pWheel, int pClicks, void* pRefcon);

    void readWindowRect(XPLMWindowID pWindow);
    void layout();
    void draw();
    void drawMapTexture();
    void drawButton(const Rect& pRect, const char* pLabel, bool pHighlighted);
    void fillRect(const Rect& pRect, float pR, float pG, float pB, float pA) const;
    void frameRect(const Rect& pRect, float pR, float pG, float pB) const;
    void drawCenteredText(int pCenterX, int pCenterY, const char* pText, float pColor[3]) const;
    int handleClick(int pX, int pY, XPLMMouseStatus pStatus);
    int handleRightClick(int pX, int pY, XPLMMouseStatus pStatus);
    int handleWheel(int pX, int pY, int pClicks);
    void handleCursor(int pX, int pY);
    void handleKey(char pKey, XPLMKeyFlags pFlags, char pVirtualKey, bool pLosingFocus);
    void syncKeyboardFocus(bool pTake);
    void press(Button pButton);

    XPLMWindowID mWindow = nullptr;
    Rect mWindowRect;
    Rect mMapRect;
    Rect mButtons[kButtonCount];
    XMapApiCalls mApi;
    std::unique_ptr<MultiXMapSelfTest> mSelfTest;
    bool mMapPress = false;
    bool mMapRightPress = false;
    Button mPressed = Button::panUp;
    double mPressTime = -1000.0;
    std::string mStatus = "Starting map...";
};

void UiPlugin::enable()
{
    XPLMCreateWindow_t tParams;
    std::memset(&tParams, 0, sizeof(tParams));
    tParams.structSize = sizeof(tParams);
    tParams.left = 80;
    tParams.top = 900;
    tParams.right = 880;
    tParams.bottom = 380;
    tParams.visible = 1;
    tParams.drawWindowFunc = drawCallback;
    tParams.handleKeyFunc = keyCallback;
    tParams.handleMouseClickFunc = clickCallback;
    tParams.handleRightClickFunc = rightClickCallback;
    tParams.handleCursorFunc = cursorCallback;
    tParams.handleMouseWheelFunc = wheelCallback;
    tParams.refcon = this;
    tParams.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;
    tParams.layer = xplm_WindowLayerFloatingWindows;
    mWindow = XPLMCreateWindowEx(&tParams);
    if (!mWindow)
    {
        XPLMDebugString("[plugin-api-test] ERROR: could not create the window\n");
        return;
    }
    XPLMSetWindowPositioningMode(mWindow, xplm_WindowPositionFree, -1);
    XPLMSetWindowResizingLimits(mWindow, 560, 400, 4000, 4000);
    XPLMSetWindowTitle(mWindow, const_cast<char*>("XMap API UI Test"));
    XPLMDebugString("[plugin-api-test] window created\n");

    if (!mApi.initLibrary("[plugin-api-test]", findPluginDirectory()))
    {
        mStatus = "PrimeMap Init FAILED (see Log.txt)";
        return;
    }
    readWindowRect(mWindow);
    layout();
    const prime::Geometry tGeometry{mMapRect.mLeft, mMapRect.mTop, mMapRect.width(), mMapRect.height()};
    if (mApi.openXMap(tGeometry, prime::WindowStyle::Texture, true, true))
    {
        mApi.setTextureSize(mMapRect.width(), mMapRect.height());
        mApi.setFollowAircraft(false);
        mApi.setCenter(kStartLat, kStartLon);
        mApi.setZoom(kStartZoom);
        mStatus = "XMap drawn into a texture inside this window";
    }
    else
    {
        mStatus = "XMap open FAILED (see Log.txt)";
    }
    mSelfTest = std::make_unique<MultiXMapSelfTest>();
    if (!mSelfTest->startIfRequested(mApi.manager()->getDataDirectory(), mApi.manager()))
    {
        mSelfTest.reset();
    }
}

void UiPlugin::disable()
{
    mSelfTest.reset();
    if (mWindow && XPLMHasKeyboardFocus(mWindow))
    {
        XPLMTakeKeyboardFocus(nullptr);
    }
    mApi.closeXMap();
    mApi.shutdownLibrary();
    if (mWindow)
    {
        XPLMDestroyWindow(mWindow);
        mWindow = nullptr;
    }
}

void UiPlugin::readWindowRect(XPLMWindowID pWindow)
{
    XPLMGetWindowGeometry(pWindow, &mWindowRect.mLeft, &mWindowRect.mTop, &mWindowRect.mRight,
                          &mWindowRect.mBottom);
}

void UiPlugin::layout()
{
    const int tWindowBottom = mWindowRect.mBottom;
    const int tWindowTop = mWindowRect.mTop;
    const int tWindowLeft = mWindowRect.mLeft;
    const int tWindowRight = mWindowRect.mRight;

    mMapRect.mLeft = tWindowLeft + kMargin + kSideWidth;
    mMapRect.mRight = tWindowRight - kMargin - kSideWidth;
    mMapRect.mTop = tWindowTop - kMargin - kTopHeight;
    mMapRect.mBottom = tWindowBottom + kMargin + kBottomHeight;

    const int tMapCenterX = (mMapRect.mLeft + mMapRect.mRight) / 2;
    const int tMapCenterY = (mMapRect.mTop + mMapRect.mBottom) / 2;
    const int tTopRowCenterY = (mMapRect.mTop + tWindowTop - kMargin) / 2;
    const int tBottomRowCenterY = (mMapRect.mBottom + tWindowBottom + kMargin) / 2;

    auto tSetButton = [this](Button pButton, int pCenterX, int pCenterY)
    {
        Rect& tRect = mButtons[static_cast<int>(pButton)];
        tRect.mLeft = pCenterX - kButtonHalfW;
        tRect.mRight = pCenterX + kButtonHalfW;
        tRect.mBottom = pCenterY - kButtonHalfH;
        tRect.mTop = pCenterY + kButtonHalfH;
    };

    tSetButton(Button::panUp, tMapCenterX, tTopRowCenterY);
    tSetButton(Button::panDown, tMapCenterX, tBottomRowCenterY);
    tSetButton(Button::panLeft, tWindowLeft + kMargin + kSideWidth / 2, tMapCenterY);
    tSetButton(Button::panRight, tWindowRight - kMargin - kSideWidth / 2, tMapCenterY);
    tSetButton(Button::zoomIn, mMapRect.mLeft + 60, tBottomRowCenterY);
    tSetButton(Button::zoomOut, mMapRect.mLeft + 180, tBottomRowCenterY);
}

void UiPlugin::fillRect(const Rect& pRect, float pR, float pG, float pB, float pA) const
{
    glColor4f(pR, pG, pB, pA);
    glBegin(GL_QUADS);
    glVertex2i(pRect.mLeft, pRect.mBottom);
    glVertex2i(pRect.mRight, pRect.mBottom);
    glVertex2i(pRect.mRight, pRect.mTop);
    glVertex2i(pRect.mLeft, pRect.mTop);
    glEnd();
}

void UiPlugin::frameRect(const Rect& pRect, float pR, float pG, float pB) const
{
    glColor4f(pR, pG, pB, 1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(pRect.mLeft + 1, pRect.mBottom + 1);
    glVertex2i(pRect.mRight - 1, pRect.mBottom + 1);
    glVertex2i(pRect.mRight - 1, pRect.mTop - 1);
    glVertex2i(pRect.mLeft + 1, pRect.mTop - 1);
    glEnd();
}

void UiPlugin::drawCenteredText(int pCenterX, int pCenterY, const char* pText, float pColor[3]) const
{
    const float tWidth = XPLMMeasureString(xplmFont_Basic, pText, static_cast<int>(std::strlen(pText)));
    XPLMDrawString(pColor, pCenterX - static_cast<int>(tWidth / 2.0f), pCenterY - 6,
                   const_cast<char*>(pText), nullptr, xplmFont_Basic);
}

void UiPlugin::drawButton(const Rect& pRect, const char* pLabel, bool pHighlighted)
{
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);
    if (pHighlighted)
    {
        fillRect(pRect, 0.30f, 0.50f, 0.34f, 1.0f);
    }
    else
    {
        fillRect(pRect, 0.26f, 0.29f, 0.33f, 1.0f);
    }
    frameRect(pRect, 0.55f, 0.58f, 0.62f);
    float tWhite[3] = {1.0f, 1.0f, 1.0f};
    drawCenteredText((pRect.mLeft + pRect.mRight) / 2, (pRect.mTop + pRect.mBottom) / 2, pLabel, tWhite);
}

void UiPlugin::drawMapTexture()
{
    if (!mApi.renderTexture())
    {
        return;
    }
    const unsigned int tTexture = mApi.getTextureId();
    if (tTexture == 0)
    {
        return;
    }
    XPLMSetGraphicsState(0, 1, 0, 0, 0, 0, 0);
    XPLMBindTexture2d(static_cast<int>(tTexture), 0);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2i(mMapRect.mLeft, mMapRect.mBottom);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2i(mMapRect.mRight, mMapRect.mBottom);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2i(mMapRect.mRight, mMapRect.mTop);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2i(mMapRect.mLeft, mMapRect.mTop);
    glEnd();
}

void UiPlugin::draw()
{
    readWindowRect(mWindow);
    layout();
    mApi.setTextureSize(mMapRect.width(), mMapRect.height());
    syncKeyboardFocus(false);

    GLboolean tCullWasOn = GL_FALSE;
    glGetBooleanv(GL_CULL_FACE, &tCullWasOn);
    glDisable(GL_CULL_FACE);

    XPLMDrawTranslucentDarkBox(mWindowRect.mLeft, mWindowRect.mTop, mWindowRect.mRight, mWindowRect.mBottom);

    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);
    fillRect(mMapRect, 0.09f, 0.11f, 0.13f, 1.0f);
    float tGray[3] = {0.70f, 0.72f, 0.75f};
    if (mApi.isOpen())
    {
        drawMapTexture();
    }
    else
    {
        drawCenteredText((mMapRect.mLeft + mMapRect.mRight) / 2, (mMapRect.mTop + mMapRect.mBottom) / 2,
                         "MAP AREA (map not open)", tGray);
    }
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);
    frameRect(mMapRect, 0.45f, 0.48f, 0.52f);

    float tWhite[3] = {1.0f, 1.0f, 1.0f};
    XPLMDrawString(tWhite, mWindowRect.mLeft + kMargin, mWindowRect.mTop - kMargin - 16,
                   const_cast<char*>("XMap API UI Test - step 3 (map texture)"), nullptr, xplmFont_Basic);

    const double tNow = XPLMGetElapsedTime();
    for (int tIndex = 0; tIndex < kButtonCount; ++tIndex)
    {
        const bool tHighlighted =
            static_cast<Button>(tIndex) == mPressed && (tNow - mPressTime) < kPressFlash;
        drawButton(mButtons[tIndex], kButtonLabels[tIndex], tHighlighted);
    }

    XPLMDrawString(tWhite, mMapRect.mLeft + 260, mWindowRect.mBottom + kMargin + 14,
                   const_cast<char*>(mStatus.c_str()), nullptr, xplmFont_Basic);

    if (tCullWasOn == GL_TRUE)
    {
        glEnable(GL_CULL_FACE);
    }
}

void UiPlugin::press(Button pButton)
{
    mPressed = pButton;
    mPressTime = XPLMGetElapsedTime();
    switch (pButton)
    {
        case Button::panUp:
        {
            mApi.panUp();
            break;
        }
        case Button::panDown:
        {
            mApi.panDown();
            break;
        }
        case Button::panLeft:
        {
            mApi.panLeft();
            break;
        }
        case Button::panRight:
        {
            mApi.panRight();
            break;
        }
        case Button::zoomIn:
        {
            mApi.zoomIn();
            break;
        }
        case Button::zoomOut:
        {
            mApi.zoomOut();
            break;
        }
        default:
        {
            break;
        }
    }
    mStatus = std::string("Last action: ") + kButtonNames[static_cast<int>(pButton)];
    XPLMDebugString("[plugin-api-test] button pressed: ");
    XPLMDebugString(kButtonNames[static_cast<int>(pButton)]);
    XPLMDebugString("\n");
}

int UiPlugin::handleClick(int pX, int pY, XPLMMouseStatus pStatus)
{
    if (pStatus == xplm_MouseDown)
    {
        for (int tIndex = 0; tIndex < kButtonCount; ++tIndex)
        {
            if (mButtons[tIndex].contains(pX, pY))
            {
                press(static_cast<Button>(tIndex));
                return 1;
            }
        }
        mMapPress = mApi.isOpen() && mMapRect.contains(pX, pY);
    }
    if (mMapPress)
    {
        prime::MouseAction tAction = prime::MouseAction::Down;
        if (pStatus == xplm_MouseDrag)
        {
            tAction = prime::MouseAction::Drag;
        }
        else if (pStatus == xplm_MouseUp)
        {
            tAction = prime::MouseAction::Up;
            mMapPress = false;
        }
        mApi.injectMouse(pX - mMapRect.mLeft, pY - mMapRect.mBottom, tAction, prime::MouseButton::Left);
        syncKeyboardFocus(tAction == prime::MouseAction::Down);
    }
    return 1;
}

int UiPlugin::handleRightClick(int pX, int pY, XPLMMouseStatus pStatus)
{
    if (pStatus == xplm_MouseDown)
    {
        mMapRightPress = mApi.isOpen() && mMapRect.contains(pX, pY);
    }
    if (mMapRightPress)
    {
        prime::MouseAction tAction = prime::MouseAction::Down;
        if (pStatus == xplm_MouseDrag)
        {
            tAction = prime::MouseAction::Drag;
        }
        else if (pStatus == xplm_MouseUp)
        {
            tAction = prime::MouseAction::Up;
            mMapRightPress = false;
        }
        mApi.injectMouse(pX - mMapRect.mLeft, pY - mMapRect.mBottom, tAction, prime::MouseButton::Right);
    }
    return 1;
}

int UiPlugin::handleWheel(int pX, int pY, int pClicks)
{
    if (!mApi.isOpen() || !mMapRect.contains(pX, pY))
    {
        return 0;
    }
    mApi.injectWheel(pX - mMapRect.mLeft, pY - mMapRect.mBottom, pClicks);
    return 1;
}

void UiPlugin::handleCursor(int pX, int pY)
{
    if (mApi.isOpen() && mMapRect.contains(pX, pY))
    {
        mApi.injectMouseMove(pX - mMapRect.mLeft, pY - mMapRect.mBottom);
    }
}

void UiPlugin::handleKey(char pKey, XPLMKeyFlags pFlags, char pVirtualKey, bool pLosingFocus)
{
    if (pLosingFocus)
    {
        return;
    }
    mApi.injectKey(pKey, static_cast<int>(pFlags), pVirtualKey);
    syncKeyboardFocus(false);
}

void UiPlugin::syncKeyboardFocus(bool pTake)
{
    const bool tWants = mApi.wantsKeyboard();
    const bool tHas = XPLMHasKeyboardFocus(mWindow) != 0;
    if (pTake && tWants && !tHas)
    {
        XPLMTakeKeyboardFocus(mWindow);
    }
    else if (!tWants && tHas)
    {
        XPLMTakeKeyboardFocus(nullptr);
    }
}

void UiPlugin::drawCallback(XPLMWindowID , void* pRefcon)
{
    static_cast<UiPlugin*>(pRefcon)->draw();
}

int UiPlugin::clickCallback(XPLMWindowID , int pX, int pY, XPLMMouseStatus pStatus, void* pRefcon)
{
    return static_cast<UiPlugin*>(pRefcon)->handleClick(pX, pY, pStatus);
}

int UiPlugin::rightClickCallback(XPLMWindowID , int pX, int pY, XPLMMouseStatus pStatus, void* pRefcon)
{
    return static_cast<UiPlugin*>(pRefcon)->handleRightClick(pX, pY, pStatus);
}

void UiPlugin::keyCallback(XPLMWindowID , char pKey, XPLMKeyFlags pFlags, char pVirtualKey, void* pRefcon,
                           int pLosingFocus)
{
    static_cast<UiPlugin*>(pRefcon)->handleKey(pKey, pFlags, pVirtualKey, pLosingFocus != 0);
}

XPLMCursorStatus UiPlugin::cursorCallback(XPLMWindowID , int pX, int pY, void* pRefcon)
{
    static_cast<UiPlugin*>(pRefcon)->handleCursor(pX, pY);
    return xplm_CursorDefault;
}

int UiPlugin::wheelCallback(XPLMWindowID , int pX, int pY, int pWheel, int pClicks, void* pRefcon)
{
    if (pWheel != 0)
    {
        return 0;
    }
    return static_cast<UiPlugin*>(pRefcon)->handleWheel(pX, pY, pClicks);
}

std::unique_ptr<UiPlugin> gPlugin;
}

PLUGIN_API int XPluginStart(char* pOutName, char* pOutSignature, char* pOutDescription)
{
    std::strcpy(pOutName, "Plugin API Test");
    std::strcpy(pOutSignature, "prime.plugin_api_test");
    std::strcpy(pOutDescription, "UI skeleton for testing the Prime XMap API");
    return 1;
}

PLUGIN_API void XPluginStop()
{
}

PLUGIN_API int XPluginEnable()
{
    gPlugin = std::make_unique<UiPlugin>();
    gPlugin->enable();
    return 1;
}

PLUGIN_API void XPluginDisable()
{
    if (gPlugin)
    {
        gPlugin->disable();
        gPlugin.reset();
    }
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID , int , void* )
{
}
