// PluginMain.cpp
// Prime XMap Plugin'in ince kabugu: X-Plane giris noktalari, menu ve prime/map/* komutlari.
// Haritaya SADECE genel API (PrimeMap.h) uzerinden erisir.
//
// Ayar dosyalarini kutuphane yonetir (eklenti dosya okumaz / yazmaz). 0.22 ve oncesi
// eklenti klasorundeki eski dosyalar ilk acilista haritanin ayar klasorune kopyalanir.
//
// akis(fabrika): addMap() -> setName / setStyle / ... -> initialize(). Ikinci harita ayni
// yolla, baska bir adla acilir: iki harita birbirinden bagimsiz gezilir ve ayarlanir.
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>

#include "XPLMDefs.h"
#include "XPLMDisplay.h"
#include "XPLMMenus.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#include "Settings.h"
#include <prime_map_api/PrimeMap.h>

namespace
{

const char* kVersion = "0.24";  // CMake project(VERSION) ile ayni olmali
const char* kMainMapName = "main";      // ayar klasoru: maps/main/
const char* kSecondMapName = "second";  // ayar klasoru: maps/second/
const char* kLegacySettingsFile = "map_settings.ini";  // 0.22 ve oncesi: eklenti klasorunun kokunde
const char* kLegacyFiles[] = {"map_settings.ini", "map_drawings.txt", "terrain_colors.ini"};

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
    void migrateLegacyFiles() const;
    void openMainMap();
    void toggleSecondMap();
    void createCommands();
    void createMenu();
    void toggleDiagnostics();
    static std::string findPluginDirectory();

    std::string mDirectory;
    prime::XMap* mMap = nullptr;              // ana harita (maps/main)
    prime::XMap* mSecondMap = nullptr;        // istege bagli ikinci harita (maps/second)
    prime::XMapManager* mManager = nullptr;   // sahibi; destroyXMapManager ile birakilir

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
    mDirectory = findPluginDirectory();
    migrateLegacyFiles();

    if (!mManager)
    {
        mManager = prime::XMapManagerFactory::createXMapManager();
    }
    mManager->setLogPrefix("[prime-map-plugin]");  // Log.txt satirlari eskisiyle ayni
    // Veri klasoru verilmedi: kutuphane varsayilanini kullanir (%LOCALAPPDATA%/prime/prime-xmap).
    // prime_map.ini henuz yoksa eski map_settings.ini'nin [cache] degerleri ilk deger olur
    Settings tLegacy;
    if (tLegacy.load(mDirectory + kLegacySettingsFile))
    {
        mManager->setTerrainCacheMb(tLegacy.getInt("cache", "terrain_mb", 0));
        mManager->setPrefetchTiles(tLegacy.getInt("cache", "prefetch_tiles", 0));
    }
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
    // Haritalar birakilinca ayarlari kutuphane tarafindan yazilir ve pencereleri kapanir
    if (mSecondMap)
    {
        mManager->removeMap(mSecondMap);
        mSecondMap = nullptr;
    }
    if (mMap)
    {
        mManager->removeMap(mMap);
        mMap = nullptr;
    }
    if (mManager)
    {
        mManager->shutdown();
        prime::XMapManagerFactory::destroyXMapManager(mManager);  // kalan haritalar da birakilir
        mManager = nullptr;
    }
    if (mMenu)
    {
        XPLMDestroyMenu(mMenu);
        mMenu = nullptr;
    }
}

void PrimeMapPlugin::migrateLegacyFiles() const
{
    // 0.22 ve oncesi dosyalar eklenti klasorunun kokundeydi; 0.23'te harita basina maps/<ad>/ altinda.
    // Yeni klasor henuz yoksa eski dosyalar oraya kopyalanir (eskiler yerinde kalir: geri donus icin).
    const std::filesystem::path tNewDirectory = std::filesystem::path(mDirectory) / "maps" / kMainMapName;
    std::error_code tError;
    if (std::filesystem::exists(tNewDirectory / kLegacySettingsFile, tError) ||
        !std::filesystem::exists(std::filesystem::path(mDirectory) / kLegacySettingsFile, tError))
    {
        return;
    }
    std::filesystem::create_directories(tNewDirectory, tError);
    for (const char* tName : kLegacyFiles)
    {
        const std::filesystem::path tOld = std::filesystem::path(mDirectory) / tName;
        if (std::filesystem::exists(tOld, tError))
        {
            std::filesystem::copy_file(tOld, tNewDirectory / tName,
                                       std::filesystem::copy_options::skip_existing, tError);
        }
    }
    debugLine("settings of an older version copied to " + tNewDirectory.string());
}

void PrimeMapPlugin::openMainMap()
{
    // Fabrika: once nesne, sonra ayarlar, en son initialize. Burada verilenler ILK acilisin
    // varsayilanlaridir; maps/main/map_settings.ini varsa initialize onu okur (kullanicinin son hali).
    if (!mManager)
    {
        return;
    }
    mMap = mManager->addMap();
    if (!mMap)
    {
        return;
    }
    mMap->setName(kMainMapName);
    mMap->setStyle(prime::WindowStyle::Standalone);
    mMap->setTitle("Prime XMap");
    mMap->setIsUiVisible(true);
    mMap->setIsMouseInputEnabled(true);
    mMap->setVisible(false);  // ilk kurulumda gizli: menuden / komutla acilir
    mMap->initialize();       // yer verilmedi: kutuphane ekranin ortasina, %70 boyutunda acar
}

void PrimeMapPlugin::toggleSecondMap()
{
    // Coklu harita ornegi: ikinci harita ilkinden bagimsizdir (gorunum, mod, katmanlar, cizimler,
    // zemin ayarlari, dosyalar); onbellek ortaktir. Kapatilinca nesne birakilir (ayarlari yazilir).
    if (mSecondMap)
    {
        if (mManager)
        {
            mManager->removeMap(mSecondMap);  // ayarlari yazilir, pencere kapanir
        }
        mSecondMap = nullptr;
        return;
    }
    if (!mManager)
    {
        return;
    }
    mSecondMap = mManager->addMap();
    if (!mSecondMap)
    {
        return;
    }
    mSecondMap->setName(kSecondMapName);
    mSecondMap->setStyle(prime::WindowStyle::Standalone);
    mSecondMap->setTitle("Prime XMap 2");
    mSecondMap->setIsUiVisible(true);
    mSecondMap->setIsMouseInputEnabled(true);
    mSecondMap->setMode(prime::MapMode::Vfr);  // ilk acilista ana haritadan farkli gorunsun
    if (mMap)
    {
        // Ilk acilista ana haritanin yaninda, yari boyutta (sonraki acilislarda dosyadaki yer)
        const prime::Geometry tMain = mMap->getGeometry();
        mSecondMap->setGeometry({tMain.mLeft + 40, tMain.mTop - 40, tMain.mWidth * 2 / 3, tMain.mHeight * 2 / 3});
    }
    mSecondMap->initialize();
    mSecondMap->setVisible(true);  // dosyada gizli kaydedilmis olsa da menuden acilinca gorunsun
}

void PrimeMapPlugin::createCommands()
{
    // Kullanici bu komutlari Settings -> Keyboard'da "prime/map" ile arayip tusa atayabilir
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
    // Plugins > Prime XMap Plugin
    XPLMMenuID tPluginsMenu = XPLMFindPluginsMenu();
    const int tSlot = XPLMAppendMenuItem(tPluginsMenu, "Prime XMap Plugin", nullptr, 0);
    mMenu = XPLMCreateMenu("Prime XMap Plugin", tPluginsMenu, tSlot, menuCallback, this);
    XPLMAppendMenuItemWithCommand(mMenu, "Show / hide map", mToggleCommand);
    XPLMAppendMenuItemWithCommand(mMenu, "Show / hide controls", mControlsCommand);
    XPLMAppendMenuItemWithCommand(mMenu, "Next map mode", mNextModeCommand);
    XPLMAppendMenuItemWithCommand(mMenu, "Open / close second map", mSecondMapCommand);
    // Karsilastirma icin X-Plane'in kendi haritasi (X-Plane'in hazir komutu)
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
    if (!mMap)
    {
        return;
    }
    mMap->setDiagnostics(!mMap->isDiagnosticsOn());
    if (mMenu && mDiagnosticsItem >= 0)
    {
        XPLMCheckMenuItem(mMenu, mDiagnosticsItem, mMap->isDiagnosticsOn() ? xplm_Menu_Checked : xplm_Menu_Unchecked);
    }
}

std::string PrimeMapPlugin::findPluginDirectory()
{
    // .../Resources/plugins/prime-map-plugin/64/win.xpl -> .../Resources/plugins/prime-map-plugin/
    std::array<char, 2048> tXplPath = {};  // SDK: en az 256 bayt
    XPLMGetPluginInfo(XPLMGetMyID(), nullptr, tXplPath.data(), nullptr, nullptr);
    std::string tPath = tXplPath.data();
    for (int tLevel = 0; tLevel < 2; ++tLevel)  // dosya adini ve "64" klasorunu at
    {
        const size_t tCut = tPath.find_last_of("/\\");
        if (tCut != std::string::npos)
        {
            tPath.erase(tCut);
        }
    }
    return tPath + XPLMGetDirectorySeparator();
}

// --- Callback'ler ------------------------------------------------------------

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
    else if (pItem == kMenuClearTrack && mMap)
    {
        mMap->clearTrack();
    }
}

int PrimeMapPlugin::onCommand(XPLMCommandRef pCommand, XPLMCommandPhase pPhase)
{
    if (pPhase != xplm_CommandBegin || !mMap)
    {
        return 0;
    }
    if (pCommand == mToggleCommand)
    {
        mMap->setVisible(!mMap->isVisible());
    }
    else if (pCommand == mControlsCommand)
    {
        mMap->setIsUiVisible(!mMap->isUiVisible());
    }
    else if (pCommand == mDiagnosticsCommand)
    {
        toggleDiagnostics();
    }
    else if (pCommand == mNextModeCommand)
    {
        mMap->nextMode();
    }
    else if (pCommand == mPrecacheCommand)
    {
        mMap->precacheVisibleArea();
    }
    else if (pCommand == mCancelPrecacheCommand)
    {
        mMap->cancelPrecache();
    }
    else if (pCommand == mSecondMapCommand)
    {
        toggleSecondMap();
    }
    return 0;  // komut bizim; baska kimse islemesin
}

std::unique_ptr<PrimeMapPlugin> gPlugin;

}  // namespace

// ============================================================================
// X-Plane'in cagirdigi giris noktalari
// ============================================================================

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

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID /*pFrom*/, int /*pMessage*/, void* /*pParam*/)
{
}
