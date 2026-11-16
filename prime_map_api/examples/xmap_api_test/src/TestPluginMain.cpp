// TestPluginMain.cpp
// Prime XMap API test eklentisi (ayri bir X-Plane eklentisi; kutuphaneyi CMake hedefi olarak kullanir).
//
//   XPluginEnable : kutuphane + Embedded harita (UI ve fare kapali), otomatik test baslar
//   flight loop   : otomatik test adimlari (sonuclar Log.txt'de "[xmap-api-test]")
//   menu          : Plugins > XMap API Test: ayni XMapApiCalls metotlari disaridan, elle
//   XPluginDisable: harita kapanir, kutuphane durur
#include <cstring>
#include <memory>
#include <vector>

#include "XPLMDefs.h"
#include "XPLMMenus.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#include "XMapApiCalls.h"
#include "SelfTest.h"

namespace
{

class TestPlugin
{
public:
    void enable();
    void disable();

private:
    struct MenuAction
    {
        const char* mLabel;
        void (TestPlugin::*mRun)();
    };

    static void menuCallback(void* pMenuRef, void* pItemRef);
    static float flightLoop(float pSinceLastCall, float pSinceLastLoop, int pCounter, void* pRefcon);

    void zoomIn()
    {
        mCalls.zoomIn();
    }
    void zoomOut()
    {
        mCalls.zoomOut();
    }
    void panLeft()
    {
        mCalls.panLeft();
    }
    void panRight()
    {
        mCalls.panRight();
    }
    void panUp()
    {
        mCalls.panUp();
    }
    void panDown()
    {
        mCalls.panDown();
    }
    void nextMode()
    {
        mCalls.nextMode();
    }
    void goToKsea()
    {
        mCalls.goToAirport("KSEA");
    }
    void toggleAirways()
    {
        mCalls.toggleLayer(prime::XMapLayer::AirwaysGroup);
    }
    void toggleUi()
    {
        mCalls.setUiVisible(!mCalls.isUiVisible());
    }
    void toggleMouse()
    {
        mCalls.setMouseInput(!mCalls.isMouseInputEnabled());
    }
    void moveRight()
    {
        prime::Geometry tGeometry = mCalls.getGeometry();
        tGeometry.mLeft += 100;
        mCalls.setGeometry(tGeometry);
    }
    void grow()
    {
        prime::Geometry tGeometry = mCalls.getGeometry();
        tGeometry.mWidth += 100;
        tGeometry.mHeight += 60;
        mCalls.setGeometry(tGeometry);
    }
    void toggleVisible()
    {
        mCalls.setVisible(!mCalls.isVisible());
    }

    XMapApiCalls mCalls;
    SelfTest mTest{mCalls};
    XPLMMenuID mMenu = nullptr;
    std::vector<MenuAction> mActions;
};

void TestPlugin::enable()
{
    mCalls.initLibrary("[xmap-api-test-lib]");
    mCalls.openXMap({150, 750, 640, 420}, prime::WindowStyle::Embedded, false, false);
    mTest.start(XPLMGetElapsedTime());

    mActions = {
        {"Zoom in", &TestPlugin::zoomIn},        {"Zoom out", &TestPlugin::zoomOut},
        {"Pan left", &TestPlugin::panLeft},      {"Pan right", &TestPlugin::panRight},
        {"Pan up", &TestPlugin::panUp},          {"Pan down", &TestPlugin::panDown},
        {"Next mode", &TestPlugin::nextMode},    {"Go to KSEA", &TestPlugin::goToKsea},
        {"Toggle airways", &TestPlugin::toggleAirways}, {"Toggle UI", &TestPlugin::toggleUi},
        {"Toggle mouse input", &TestPlugin::toggleMouse}, {"Move window right", &TestPlugin::moveRight},
        {"Grow window", &TestPlugin::grow},      {"Show / hide", &TestPlugin::toggleVisible},
    };
    const int tSlot = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "XMap API Test", nullptr, 0);
    mMenu = XPLMCreateMenu("XMap API Test", XPLMFindPluginsMenu(), tSlot, menuCallback, this);
    for (size_t tIndex = 0; tIndex < mActions.size(); ++tIndex)
    {
        XPLMAppendMenuItem(mMenu, mActions[tIndex].mLabel, reinterpret_cast<void*>(tIndex), 0);
    }
    XPLMRegisterFlightLoopCallback(flightLoop, -1.0f, this);
}

void TestPlugin::disable()
{
    XPLMUnregisterFlightLoopCallback(flightLoop, this);
    if (mMenu)
    {
        XPLMDestroyMenu(mMenu);
        mMenu = nullptr;
    }
    mCalls.closeXMap();
    mCalls.shutdownLibrary();
}

void TestPlugin::menuCallback(void* pMenuRef, void* pItemRef)
{
    TestPlugin* tSelf = static_cast<TestPlugin*>(pMenuRef);
    const size_t tIndex = reinterpret_cast<size_t>(pItemRef);
    if (tIndex < tSelf->mActions.size())
    {
        (tSelf->*tSelf->mActions[tIndex].mRun)();
    }
}

float TestPlugin::flightLoop(float /*pSinceLastCall*/, float /*pSinceLastLoop*/, int /*pCounter*/, void* pRefcon)
{
    TestPlugin* tSelf = static_cast<TestPlugin*>(pRefcon);
    if (!tSelf->mTest.isDone())
    {
        tSelf->mTest.step(XPLMGetElapsedTime());
    }
    return -1.0f;
}

std::unique_ptr<TestPlugin> gPlugin;

}  // namespace

PLUGIN_API int XPluginStart(char* pOutName, char* pOutSignature, char* pOutDescription)
{
    std::strcpy(pOutName, "XMap API Test");
    std::strcpy(pOutSignature, "prime.xmap_api_test");
    std::strcpy(pOutDescription, "Tests every Prime XMap API function");
    return 1;
}

PLUGIN_API void XPluginStop()
{
}

PLUGIN_API int XPluginEnable()
{
    gPlugin = std::make_unique<TestPlugin>();
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

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID /*pFrom*/, int /*pMessage*/, void* /*pParam*/)
{
}
