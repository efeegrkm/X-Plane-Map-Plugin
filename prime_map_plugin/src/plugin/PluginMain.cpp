#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#include "XPLMDefs.h"
#include "XPLMDisplay.h"
#include "XPLMMenus.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#include <prime_map_api/PrimeMap.h>

namespace
{
#ifndef PRIME_MAP_VERSION
#define PRIME_MAP_VERSION "1.1.1"
#endif
const char* kVersion = PRIME_MAP_VERSION;
const char* kMainMapName = "main";
const char* kSecondMapName = "second";

void debugLine(const std::string& pText)
{
    XPLMDebugString(("[prime-map-plugin] " + pText + "\n").c_str());
}

class PrimeMapPlugin
{
public:
    void start();
    void stop();

private:
    enum MenuItem
    {
        kMenuDiagnostics = 1,
        kMenuClearTrack,
    };

    static void menuCallback(void* pMenuRef, void* pItemRef);
    static int commandCallback(XPLMCommandRef pCommand, XPLMCommandPhase pPhase, void* pRefcon);

    void onMenu(int pItem);
    int onCommand(XPLMCommandRef pCommand, XPLMCommandPhase pPhase);
    void openMainMap();
    void toggleSecondMap();
    void createCommands();
    void createMenu();
    void toggleDiagnostics();

    prime::XMap* mXMap = nullptr;
    prime::XMap* mSecondXMap = nullptr;
    prime::XMapManager* mManager = nullptr;

    XPLMMenuID mMenu = nullptr;
    int mDiagnosticsItem = -1;
    XPLMCommandRef mToggleCommand = nullptr;
    XPLMCommandRef mControlsCommand = nullptr;
    XPLMCommandRef mDiagnosticsCommand = nullptr;
    XPLMCommandRef mNextModeCommand = nullptr;
    XPLMCommandRef mPrecacheCommand = nullptr;
    XPLMCommandRef mCancelPrecacheCommand = nullptr;
    XPLMCommandRef mSecondMapCommand = nullptr;
};

void PrimeMapPlugin::start()
{
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);
    if (!mManager)
    {
        mManager = prime::XMapManagerFactory::createXMapManager();
    }
    mManager->setLogPrefix("[prime-map-plugin]");
    mManager->initialize();

    openMainMap();
    createCommands();
    createMenu();
    debugLine("loaded (version " + std::string(kVersion) + ", API " + prime::versionString() + ")");
}

void PrimeMapPlugin::stop()
{
    for (XPLMCommandRef tCommand : {mToggleCommand, mControlsCommand, mDiagnosticsCommand, mNextModeCommand,
                                    mPrecacheCommand, mCancelPrecacheCommand, mSecondMapCommand})
    {
        XPLMUnregisterCommandHandler(tCommand, commandCallback, 1, this);
    }
    if (mSecondXMap)
    {
        mManager->removeXMap(mSecondXMap);
        mSecondXMap = nullptr;
    }
    if (mXMap)
    {
        mManager->removeXMap(mXMap);
        mXMap = nullptr;
    }
    if (mManager)
    {
        mManager->shutdown();
        prime::XMapManagerFactory::destroyXMapManager(mManager);
        mManager = nullptr;
    }
    if (mMenu)
    {
        XPLMDestroyMenu(mMenu);
        mMenu = nullptr;
    }
}

void PrimeMapPlugin::openMainMap()
{
    if (!mManager)
    {
        return;
    }
    mXMap = mManager->addXMap();
    if (!mXMap)
    {
        return;
    }
    mXMap->setName(kMainMapName);
    mXMap->setStyle(prime::WindowStyle::Standalone);
    mXMap->setTitle("Prime XMap");
    mXMap->setIsUiVisible(true);
    mXMap->setIsMouseInputEnabled(true);
    mXMap->setVisible(false);
    mXMap->initialize();
}

void PrimeMapPlugin::toggleSecondMap()
{
    if (mSecondXMap)
    {
        if (mManager)
        {
            mManager->removeXMap(mSecondXMap);
        }
        mSecondXMap = nullptr;
        return;
    }
    if (!mManager)
    {
        return;
    }
    mSecondXMap = mManager->addXMap();
    if (!mSecondXMap)
    {
        return;
    }
    mSecondXMap->setName(kSecondMapName);
    mSecondXMap->setStyle(prime::WindowStyle::Standalone);
    mSecondXMap->setTitle("Prime XMap 2");
    mSecondXMap->setIsUiVisible(true);
    mSecondXMap->setIsMouseInputEnabled(true);
    mSecondXMap->setMode(prime::XMapMode::Vfr);
    if (mXMap)
    {
        const prime::Geometry tMain = mXMap->getGeometry();
        mSecondXMap->setGeometry({tMain.mLeft + 40, tMain.mTop - 40, tMain.mWidth * 2 / 3, tMain.mHeight * 2 / 3});
    }
    mSecondXMap->initialize();
    mSecondXMap->setVisible(true);
}

void PrimeMapPlugin::createCommands()
{
    mToggleCommand = XPLMCreateCommand("prime/map/toggle", "XMap: show / hide the map window");
    mControlsCommand = XPLMCreateCommand("prime/map/controls", "XMap: show / hide the controls");
    mDiagnosticsCommand = XPLMCreateCommand("prime/map/diagnostics", "XMap: toggle diagnostics mode");
    mNextModeCommand = XPLMCreateCommand("prime/map/next_mode", "XMap: next map mode (Scenery / VFR / IFR)");
    mPrecacheCommand = XPLMCreateCommand("prime/map/precache_view", "XMap: pre-cache terrain of the visible area");
    mCancelPrecacheCommand = XPLMCreateCommand("prime/map/precache_cancel", "XMap: cancel terrain pre-cache");
    mSecondMapCommand = XPLMCreateCommand("prime/map/second_map", "XMap: open / close the second map window");
    for (XPLMCommandRef tCommand : {mToggleCommand, mControlsCommand, mDiagnosticsCommand, mNextModeCommand,
                                    mPrecacheCommand, mCancelPrecacheCommand, mSecondMapCommand})
    {
        XPLMRegisterCommandHandler(tCommand, commandCallback, 1, this);
    }
}

void PrimeMapPlugin::createMenu()
{
    XPLMMenuID tPluginsMenu = XPLMFindPluginsMenu();
    const int tSlot = XPLMAppendMenuItem(tPluginsMenu, "Prime XMap Plugin", nullptr, 0);
    mMenu = XPLMCreateMenu("Prime XMap Plugin", tPluginsMenu, tSlot, menuCallback, this);
    XPLMAppendMenuItemWithCommand(mMenu, "Show / hide map", mToggleCommand);
    XPLMAppendMenuItemWithCommand(mMenu, "Show / hide controls", mControlsCommand);
    XPLMAppendMenuItemWithCommand(mMenu, "Next map mode", mNextModeCommand);
    XPLMAppendMenuItemWithCommand(mMenu, "Open / close second map", mSecondMapCommand);
    XPLMCommandRef tXPlaneMapCommand = XPLMFindCommand("sim/map/show_current");
    if (tXPlaneMapCommand)
    {
        XPLMAppendMenuItemWithCommand(mMenu, "Show / hide X-Plane map (compare)", tXPlaneMapCommand);
    }
    XPLMAppendMenuSeparator(mMenu);
    mDiagnosticsItem =
        XPLMAppendMenuItem(mMenu, "Diagnostics mode", reinterpret_cast<void*>(intptr_t(kMenuDiagnostics)), 0);
    XPLMCheckMenuItem(mMenu, mDiagnosticsItem, xplm_Menu_Unchecked);
    XPLMAppendMenuItem(mMenu, "Clear flight track", reinterpret_cast<void*>(intptr_t(kMenuClearTrack)), 0);
    XPLMAppendMenuSeparator(mMenu);
    XPLMAppendMenuItemWithCommand(mMenu, "Pre-cache terrain of visible area", mPrecacheCommand);
    XPLMAppendMenuItemWithCommand(mMenu, "Cancel terrain pre-cache", mCancelPrecacheCommand);
}

void PrimeMapPlugin::toggleDiagnostics()
{
    if (!mXMap)
    {
        return;
    }
    mXMap->setDiagnostics(!mXMap->isDiagnosticsOn());
    if (mMenu && mDiagnosticsItem >= 0)
    {
        XPLMCheckMenuItem(mMenu, mDiagnosticsItem, mXMap->isDiagnosticsOn() ? xplm_Menu_Checked : xplm_Menu_Unchecked);
    }
}

void PrimeMapPlugin::menuCallback(void* pMenuRef, void* pItemRef)
{
    static_cast<PrimeMapPlugin*>(pMenuRef)->onMenu(static_cast<int>(reinterpret_cast<intptr_t>(pItemRef)));
}

int PrimeMapPlugin::commandCallback(XPLMCommandRef pCommand, XPLMCommandPhase pPhase, void* pRefcon)
{
    return static_cast<PrimeMapPlugin*>(pRefcon)->onCommand(pCommand, pPhase);
}

void PrimeMapPlugin::onMenu(int pItem)
{
    if (pItem == kMenuDiagnostics)
    {
        toggleDiagnostics();
    }
    else if (pItem == kMenuClearTrack && mXMap)
    {
        mXMap->clearTrack();
    }
}

int PrimeMapPlugin::onCommand(XPLMCommandRef pCommand, XPLMCommandPhase pPhase)
{
    if (pPhase != xplm_CommandBegin || !mXMap)
    {
        return 0;
    }
    if (pCommand == mToggleCommand)
    {
        mXMap->setVisible(!mXMap->isVisible());
    }
    else if (pCommand == mControlsCommand)
    {
        mXMap->setIsUiVisible(!mXMap->isUiVisible());
    }
    else if (pCommand == mDiagnosticsCommand)
    {
        toggleDiagnostics();
    }
    else if (pCommand == mNextModeCommand)
    {
        mXMap->nextMode();
    }
    else if (pCommand == mPrecacheCommand)
    {
        mXMap->precacheVisibleArea();
    }
    else if (pCommand == mCancelPrecacheCommand)
    {
        mXMap->cancelPrecache();
    }
    else if (pCommand == mSecondMapCommand)
    {
        toggleSecondMap();
    }
    return 0;
}

std::unique_ptr<PrimeMapPlugin> gPlugin;
}

PLUGIN_API int XPluginStart(char* pOutName, char* pOutSignature, char* pOutDescription)
{
    std::strcpy(pOutName, "Prime XMap Plugin");
    std::strcpy(pOutSignature, "prime.prime_map_plugin");
    std::strcpy(pOutDescription, "Layered moving map in its own window (R&D)");
    gPlugin = std::make_unique<PrimeMapPlugin>();
    gPlugin->start();
    return 1;
}

PLUGIN_API void XPluginStop()
{
    if (gPlugin)
    {
        gPlugin->stop();
        gPlugin.reset();
    }
}

PLUGIN_API int XPluginEnable()
{
    return 1;
}

PLUGIN_API void XPluginDisable()
{
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID , int , void* )
{
}
