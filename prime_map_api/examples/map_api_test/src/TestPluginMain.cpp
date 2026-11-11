// TestPluginMain.cpp
// Prime Map API test eklentisi (ayri bir X-Plane eklentisi; kutuphaneyi CMake hedefi olarak kullanir).
//
//   XPluginEnable : kutuphane + Embedded harita (UI ve fare kapali), otomatik test baslar
//   flight loop   : otomatik test adimlari (sonuclar Log.txt'de "[map-api-test]")
//   menu          : Plugins > Map API Test: ayni MapApiCalls metotlari disaridan, elle
//   XPluginDisable: harita kapanir, kutuphane durur
#include <cstring>
#include <memory>
#include <vector>

#include "XPLMDefs.h"
#include "XPLMMenus.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#include "MapApiCalls.h"
#include "SelfTest.h"

namespace
{

class TestPlugin
{
public:
    void Enable();
    void Disable();

private:
    struct MenuAction
    {
        const char* mLabel;
        void (TestPlugin::*mRun)();
    };

    static void MenuCallback(void* pMenuRef, void* pItemRef);
    static float FlightLoop(float pSinceLastCall, float pSinceLastLoop, int pCounter, void* pRefcon);

    void ZoomIn()
    {
        mCalls.ZoomIn();
    }
    void ZoomOut()
    {
        mCalls.ZoomOut();
    }
    void PanLeft()
    {
        mCalls.PanLeft();
    }
    void PanRight()
    {
        mCalls.PanRight();
    }
    void PanUp()
    {
        mCalls.PanUp();
    }
    void PanDown()
    {
        mCalls.PanDown();
    }
    void NextMode()
    {
        mCalls.NextMode();
    }
    void GoToKsea()
    {
        mCalls.GoToAirport("KSEA");
    }
    void ToggleAirways()
    {
        mCalls.ToggleLayer(PrimeMap::MapLayer::AirwaysGroup);
    }
    void ToggleUi()
    {
        mCalls.SetUiVisible(!mCalls.IsUiVisible());
    }
    void ToggleMouse()
    {
        mCalls.SetMouseInput(!mCalls.IsMouseInputEnabled());
    }
    void MoveRight()
    {
        PrimeMap::Geometry tGeometry = mCalls.GetGeometry();
        tGeometry.mLeft += 100;
        mCalls.SetGeometry(tGeometry);
    }
    void Grow()
    {
        PrimeMap::Geometry tGeometry = mCalls.GetGeometry();
        tGeometry.mWidth += 100;
        tGeometry.mHeight += 60;
        mCalls.SetGeometry(tGeometry);
    }
    void ToggleVisible()
    {
        mCalls.SetVisible(!mCalls.IsVisible());
    }

    MapApiCalls mCalls;
    SelfTest mTest{mCalls};
    XPLMMenuID mMenu = nullptr;
    std::vector<MenuAction> mActions;
};

void TestPlugin::Enable()
{
    mCalls.InitLibrary("[map-api-test-lib]");
    mCalls.OpenMap({150, 750, 640, 420}, PrimeMap::WindowStyle::Embedded, false, false);
    mTest.Start(XPLMGetElapsedTime());

    mActions = {
        {"Zoom in", &TestPlugin::ZoomIn},        {"Zoom out", &TestPlugin::ZoomOut},
        {"Pan left", &TestPlugin::PanLeft},      {"Pan right", &TestPlugin::PanRight},
        {"Pan up", &TestPlugin::PanUp},          {"Pan down", &TestPlugin::PanDown},
        {"Next mode", &TestPlugin::NextMode},    {"Go to KSEA", &TestPlugin::GoToKsea},
        {"Toggle airways", &TestPlugin::ToggleAirways}, {"Toggle UI", &TestPlugin::ToggleUi},
        {"Toggle mouse input", &TestPlugin::ToggleMouse}, {"Move window right", &TestPlugin::MoveRight},
        {"Grow window", &TestPlugin::Grow},      {"Show / hide", &TestPlugin::ToggleVisible},
    };
    const int tSlot = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "Map API Test", nullptr, 0);
    mMenu = XPLMCreateMenu("Map API Test", XPLMFindPluginsMenu(), tSlot, MenuCallback, this);
    for (size_t tIndex = 0; tIndex < mActions.size(); ++tIndex)
    {
        XPLMAppendMenuItem(mMenu, mActions[tIndex].mLabel, reinterpret_cast<void*>(tIndex), 0);
    }
    XPLMRegisterFlightLoopCallback(FlightLoop, -1.0f, this);
}

void TestPlugin::Disable()
{
    XPLMUnregisterFlightLoopCallback(FlightLoop, this);
    if (mMenu)
    {
        XPLMDestroyMenu(mMenu);
        mMenu = nullptr;
    }
    mCalls.CloseMap();
    mCalls.ShutdownLibrary();
}

void TestPlugin::MenuCallback(void* pMenuRef, void* pItemRef)
{
    TestPlugin* tSelf = static_cast<TestPlugin*>(pMenuRef);
    const size_t tIndex = reinterpret_cast<size_t>(pItemRef);
    if (tIndex < tSelf->mActions.size())
    {
        (tSelf->*tSelf->mActions[tIndex].mRun)();
    }
}

float TestPlugin::FlightLoop(float /*pSinceLastCall*/, float /*pSinceLastLoop*/, int /*pCounter*/, void* pRefcon)
{
    TestPlugin* tSelf = static_cast<TestPlugin*>(pRefcon);
    if (!tSelf->mTest.IsDone())
    {
        tSelf->mTest.Step(XPLMGetElapsedTime());
    }
    return -1.0f;
}

std::unique_ptr<TestPlugin> gPlugin;

}  // namespace

PLUGIN_API int XPluginStart(char* pOutName, char* pOutSignature, char* pOutDescription)
{
    std::strcpy(pOutName, "Map API Test");
    std::strcpy(pOutSignature, "prime.map_api_test");
    std::strcpy(pOutDescription, "Tests every Prime Map API function");
    return 1;
}

PLUGIN_API void XPluginStop()
{
}

PLUGIN_API int XPluginEnable()
{
    gPlugin = std::make_unique<TestPlugin>();
    gPlugin->Enable();
    return 1;
}

PLUGIN_API void XPluginDisable()
{
    if (gPlugin)
    {
        gPlugin->Disable();
        gPlugin.reset();
    }
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID /*pFrom*/, int /*pMessage*/, void* /*pParam*/)
{
}
