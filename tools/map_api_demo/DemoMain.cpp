// DemoMain.cpp
// Prime Map API ornek host eklentisi (map_api_demo). GNS 430 gibi bir eklentinin haritayi
// nasil gomecegini gosterir:
//
//   +-- "Demo bezel" (host'un kendi XPLM penceresi, cerceveli) ------+
//   |  +-- harita (PrimeMap::Map, Embedded, cercevesiz) ----------+  |
//   |  |                                                         |  |
//   |  +---------------------------------------------------------+  |
//   +----------------------------------------------------------------+
//
// Harita host penceresinin ic alanini her karede takip eder (SetGeometry). Host penceresine
// tiklaninca X-Plane onu one getirir; harita da ardindan BringToFront ile one alinir.
// Dugmeler yerine X-Plane komutlari: Plugins > Map API Demo menusunden ya da tusa atanarak
// (Settings > Keyboard, "ozdev/map_demo"). Gercek host kendi dugmelerini ayni cagrilara baglar.
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "XPLMDefs.h"
#include "XPLMDisplay.h"
#include "XPLMMenus.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#include "PrimeMap.h"  // tek genel baslik (SDK paketinde include/PrimeMap.h)

namespace
{

const int kBezel = 16;  // host penceresi ile harita arasindaki kenar (boxel)

class DemoPlugin
{
public:
    void Enable();
    void Disable();

private:
    struct Action
    {
        const char* mCommand;
        const char* mLabel;
        void (DemoPlugin::*mRun)();
        XPLMCommandRef mRef = nullptr;
    };

    static void DrawBezel(XPLMWindowID pWindow, void* pRefcon);
    static int ClickBezel(XPLMWindowID pWindow, int pX, int pY, XPLMMouseStatus pStatus, void* pRefcon);
    static int CommandCallback(XPLMCommandRef pCommand, XPLMCommandPhase pPhase, void* pRefcon);
    static float FlightLoop(float pSinceLastCall, float pSinceLastLoop, int pCounter, void* pRefcon);

    void FollowBezel();  // haritayi host penceresinin ic alanina yerlestir

    void ZoomIn()
    {
        mMap->ZoomIn();
    }
    void ZoomOut()
    {
        mMap->ZoomOut();
    }
    void PanLeft()
    {
        mMap->PanLeft();
    }
    void PanRight()
    {
        mMap->PanRight();
    }
    void PanUp()
    {
        mMap->PanUp();
    }
    void PanDown()
    {
        mMap->PanDown();
    }
    void NextMode()
    {
        mMap->NextMode();
    }
    void IfrLow()
    {
        mMap->SetMode(PrimeMap::MapMode::IfrLow);
    }
    void GoToKsea()
    {
        mMap->GoToAirport("KSEA");
    }
    void FitSeattle()
    {
        PrimeMap::GeoBounds tArea;
        tArea.mSouth = 47.0;
        tArea.mWest = -123.0;
        tArea.mNorth = 48.0;
        tArea.mEast = -122.0;
        mMap->FitBounds(tArea);
    }
    void ToggleAirways()
    {
        mMap->ToggleLayer(PrimeMap::MapLayer::AirwaysGroup);
    }
    void OnlyAirportsAndNavaids()
    {
        mMap->ShowOnlyLayers({PrimeMap::MapLayer::Terrain, PrimeMap::MapLayer::Runways, PrimeMap::MapLayer::Vor,
                              PrimeMap::MapLayer::Ndb, PrimeMap::MapLayer::Aircraft});
    }
    void ToggleUi()
    {
        mMap->SetUiVisible(!mMap->IsUiVisible());
    }
    void ToggleMouse()
    {
        mMap->SetMouseInput(!mMap->IsMouseInputEnabled());
    }
    void FollowAircraft()
    {
        mMap->SetFollowAircraft(true);
    }

    XPLMWindowID mBezel = nullptr;
    std::unique_ptr<PrimeMap::Map> mMap;
    XPLMMenuID mMenu = nullptr;
    std::vector<Action> mActions;
};

void DemoPlugin::Enable()
{
    // 1) Kutuphaneyi baslat: veri arka planda yuklenmeye baslar
    PrimeMap::InitConfig tInit;
    tInit.mLogPrefix = "[map-api-demo]";  // onbellek: bos = bu eklentinin klasoru/cache/
    PrimeMap::Init(tInit);

    // 2) Host'un kendi penceresi (gercek GNS'te cihazin cercevesi / ekrani)
    XPLMCreateWindow_t tParams;
    std::memset(&tParams, 0, sizeof(tParams));
    tParams.structSize = sizeof(tParams);
    tParams.left = 100;
    tParams.top = 700;
    tParams.right = 100 + 560;
    tParams.bottom = 700 - 380;
    tParams.visible = 1;
    tParams.drawWindowFunc = DrawBezel;
    tParams.handleMouseClickFunc = ClickBezel;
    tParams.handleRightClickFunc = ClickBezel;
    tParams.handleKeyFunc = [](XPLMWindowID, char, XPLMKeyFlags, char, void*, int)
    {
    };
    tParams.handleCursorFunc = [](XPLMWindowID, int, int, void*) -> XPLMCursorStatus
    {
        return xplm_CursorDefault;
    };
    tParams.handleMouseWheelFunc = [](XPLMWindowID, int, int, int, int, void*)
    {
        return 1;
    };
    tParams.refcon = this;
    tParams.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;
    tParams.layer = xplm_WindowLayerFloatingWindows;
    mBezel = XPLMCreateWindowEx(&tParams);
    XPLMSetWindowPositioningMode(mBezel, xplm_WindowPositionFree, -1);
    std::string tTitle = "Map API demo (host window)";
    XPLMSetWindowTitle(mBezel, tTitle.data());

    // 3) Haritayi ac: cercevesiz, UI ve fare kapali (host her seyi API ile yonetir)
    PrimeMap::OpenConfig tOpen;
    tOpen.mGeometry = {tParams.left + kBezel, tParams.top - kBezel, 560 - 2 * kBezel, 380 - 2 * kBezel};
    tOpen.mStyle = PrimeMap::WindowStyle::Embedded;
    tOpen.mMode = PrimeMap::MapMode::Vfr;
    tOpen.mUiVisible = false;
    tOpen.mMouseInput = false;
    mMap = PrimeMap::Map::Open(tOpen);

    // 4) Komutlar ve menu (gercek host: kendi dugmeleri)
    mActions = {
        {"ozdev/map_demo/zoom_in", "Zoom in", &DemoPlugin::ZoomIn},
        {"ozdev/map_demo/zoom_out", "Zoom out", &DemoPlugin::ZoomOut},
        {"ozdev/map_demo/pan_left", "Pan left", &DemoPlugin::PanLeft},
        {"ozdev/map_demo/pan_right", "Pan right", &DemoPlugin::PanRight},
        {"ozdev/map_demo/pan_up", "Pan up", &DemoPlugin::PanUp},
        {"ozdev/map_demo/pan_down", "Pan down", &DemoPlugin::PanDown},
        {"ozdev/map_demo/next_mode", "Next mode", &DemoPlugin::NextMode},
        {"ozdev/map_demo/ifr_low", "IFR Low mode", &DemoPlugin::IfrLow},
        {"ozdev/map_demo/goto_ksea", "Go to KSEA", &DemoPlugin::GoToKsea},
        {"ozdev/map_demo/fit_seattle", "Fit Seattle area", &DemoPlugin::FitSeattle},
        {"ozdev/map_demo/follow", "Follow aircraft", &DemoPlugin::FollowAircraft},
        {"ozdev/map_demo/toggle_airways", "Toggle airways group", &DemoPlugin::ToggleAirways},
        {"ozdev/map_demo/only_basics", "Show only terrain, runways, VOR, NDB, aircraft",
         &DemoPlugin::OnlyAirportsAndNavaids},
        {"ozdev/map_demo/toggle_ui", "Toggle map UI", &DemoPlugin::ToggleUi},
        {"ozdev/map_demo/toggle_mouse", "Toggle map mouse input", &DemoPlugin::ToggleMouse},
    };
    const int tSlot = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "Map API Demo", nullptr, 0);
    mMenu = XPLMCreateMenu("Map API Demo", XPLMFindPluginsMenu(), tSlot, nullptr, nullptr);
    for (Action& tAction : mActions)
    {
        tAction.mRef = XPLMCreateCommand(tAction.mCommand, tAction.mLabel);
        XPLMRegisterCommandHandler(tAction.mRef, CommandCallback, 1, this);
        XPLMAppendMenuItemWithCommand(mMenu, tAction.mLabel, tAction.mRef);
    }
    XPLMRegisterFlightLoopCallback(FlightLoop, -1.0f, this);
}

void DemoPlugin::Disable()
{
    XPLMUnregisterFlightLoopCallback(FlightLoop, this);
    for (Action& tAction : mActions)
    {
        XPLMUnregisterCommandHandler(tAction.mRef, CommandCallback, 1, this);
    }
    if (mMenu)
    {
        XPLMDestroyMenu(mMenu);
        mMenu = nullptr;
    }
    mMap.reset();         // 5) once harita
    PrimeMap::Shutdown(); // 6) sonra kutuphane
    if (mBezel)
    {
        XPLMDestroyWindow(mBezel);
        mBezel = nullptr;
    }
}

void DemoPlugin::FollowBezel()
{
    if (!mMap || !mBezel)
    {
        return;
    }
    // Host penceresi kapatildiysa (X) harita da gizlenir
    const bool tBezelVisible = XPLMGetWindowIsVisible(mBezel) != 0;
    if (mMap->IsVisible() != tBezelVisible)
    {
        mMap->SetVisible(tBezelVisible);
    }
    int tLeft = 0;
    int tTop = 0;
    int tRight = 0;
    int tBottom = 0;
    XPLMGetWindowGeometry(mBezel, &tLeft, &tTop, &tRight, &tBottom);
    const PrimeMap::Geometry tWanted = {tLeft + kBezel, tTop - kBezel, tRight - tLeft - 2 * kBezel,
                                        tTop - tBottom - 2 * kBezel};
    const PrimeMap::Geometry tNow = mMap->GetGeometry();
    if (tNow.mLeft != tWanted.mLeft || tNow.mTop != tWanted.mTop || tNow.mWidth != tWanted.mWidth ||
        tNow.mHeight != tWanted.mHeight)
    {
        mMap->SetGeometry(tWanted);
    }
}

void DemoPlugin::DrawBezel(XPLMWindowID /*pWindow*/, void* /*pRefcon*/)
{
    // Cerceve ve zemin X-Plane'in (RoundRectangle); harita ustunde kendi penceresinde cizilir
}

int DemoPlugin::ClickBezel(XPLMWindowID /*pWindow*/, int /*pX*/, int /*pY*/, XPLMMouseStatus pStatus,
                           void* pRefcon)
{
    DemoPlugin* tSelf = static_cast<DemoPlugin*>(pRefcon);
    if (pStatus == xplm_MouseDown && tSelf->mMap)
    {
        tSelf->mMap->BringToFront();  // host one geldi: harita yine onun ustunde kalsin
    }
    return 1;
}

int DemoPlugin::CommandCallback(XPLMCommandRef pCommand, XPLMCommandPhase pPhase, void* pRefcon)
{
    DemoPlugin* tSelf = static_cast<DemoPlugin*>(pRefcon);
    if (pPhase != xplm_CommandBegin || !tSelf->mMap)
    {
        return 0;
    }
    for (const Action& tAction : tSelf->mActions)
    {
        if (tAction.mRef == pCommand)
        {
            (tSelf->*tAction.mRun)();
        }
    }
    return 0;
}

float DemoPlugin::FlightLoop(float /*pSinceLastCall*/, float /*pSinceLastLoop*/, int /*pCounter*/, void* pRefcon)
{
    static_cast<DemoPlugin*>(pRefcon)->FollowBezel();
    return -1.0f;
}

std::unique_ptr<DemoPlugin> gDemo;

}  // namespace

PLUGIN_API int XPluginStart(char* pOutName, char* pOutSignature, char* pOutDescription)
{
    std::strcpy(pOutName, "Map API Demo");
    std::strcpy(pOutSignature, "ozdev.map_api_demo");
    std::strcpy(pOutDescription, "Example host plugin for the Prime Map API");
    return 1;
}

PLUGIN_API void XPluginStop()
{
}

PLUGIN_API int XPluginEnable()
{
    gDemo = std::make_unique<DemoPlugin>();
    gDemo->Enable();
    return 1;
}

PLUGIN_API void XPluginDisable()
{
    if (gDemo)
    {
        gDemo->Disable();
        gDemo.reset();
    }
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID /*pFrom*/, int /*pMessage*/, void* /*pParam*/)
{
}
