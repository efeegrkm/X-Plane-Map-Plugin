// PluginMain.cpp
// Prime Map Plugin'in ince kabugu: X-Plane'in cagirdigi 5 giris fonksiyonu, menu ve
// prime/map/* komutlari. Haritaya SADECE genel API (PrimeMap.h) uzerinden erisir; bu
// yuzden eklentide yapilabilen her sey API'de de vardir.
//
// 0.23: ayar dosyalarini artik KUTUPHANE yonetir (eklenti dosya okumaz / yazmaz):
//
//   <eklenti klasoru>/prime_map.ini                  merkezi ayarlar (onbellek sinirlari ...)
//   <eklenti klasoru>/maps/main/map_settings.ini     ana haritanin ayarlari
//   <eklenti klasoru>/maps/main/map_drawings.txt     ana haritanin cizimleri
//   <eklenti klasoru>/maps/main/terrain_colors.ini   ana haritanin zemin renkleri
//   <eklenti klasoru>/maps/second/...                ikinci haritanin dosyalari (menuden acilirsa)
//   <eklenti klasoru>/cache/                         ortak onbellek
//
// Eski surumlerin (0.22 ve oncesi) eklenti klasorundeki map_settings.ini / map_drawings.txt /
// terrain_colors.ini dosyalari ilk acilista maps/main/ altina KOPYALANIR (eskiler silinmez).
//
// Akis (fabrika): CreateMap() -> SetName / SetStyle / ... -> Open(). Ikinci harita ayni
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

const char* kVersion = "0.23";  // CMake project(VERSION) ile ayni olmali
const char* kMainMapName = "main";      // ayar klasoru: maps/main/
const char* kSecondMapName = "second";  // ayar klasoru: maps/second/
const char* kLegacySettingsFile = "map_settings.ini";  // 0.22 ve oncesi: eklenti klasorunun kokunde
const char* kLegacyFiles[] = {"map_settings.ini", "map_drawings.txt", "terrain_colors.ini"};

void DebugLine(const std::string& pText)
{
    XPLMDebugString(("[prime-map-plugin] " + pText + "\n").c_str());
}

class PrimeMapPlugin
{
public:
    void Start();
    void Stop();

private:
    enum MenuItem
    {
        kMenuDiagnostics = 1,
        kMenuClearTrack,
    };

    static void MenuCallback(void* pMenuRef, void* pItemRef);
    static int CommandCallback(XPLMCommandRef pCommand, XPLMCommandPhase pPhase, void* pRefcon);

    void OnMenu(int pItem);
    int OnCommand(XPLMCommandRef pCommand, XPLMCommandPhase pPhase);
    void MigrateLegacyFiles() const;
    void OpenMainMap();
    void ToggleSecondMap();
    void CreateCommands();
    void CreateMenu();
    void ToggleDiagnostics();
    static std::string FindPluginDirectory();

    std::string mDirectory;
    std::unique_ptr<prime::Map> mMap;        // ana harita (maps/main)
    std::unique_ptr<prime::Map> mSecondMap;  // istege bagli ikinci harita (maps/second)

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

void PrimeMapPlugin::Start()
{
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);
    mDirectory = FindPluginDirectory();
    MigrateLegacyFiles();

    prime::InitConfig tInit;
    tInit.mDataDirectory = mDirectory;           // prime_map.ini, maps/, cache/ eklenti klasorunde
    tInit.mLogPrefix = "[prime-map-plugin]";     // Log.txt satirlari eskisiyle ayni
    // prime_map.ini henuz yoksa eski map_settings.ini'nin [cache] degerleri ilk deger olur
    Settings tLegacy;
    if (tLegacy.Load(mDirectory + kLegacySettingsFile))
    {
        tInit.mTerrainCacheMb = tLegacy.GetInt("cache", "terrain_mb", tInit.mTerrainCacheMb);
        tInit.mPrefetchTiles = tLegacy.GetInt("cache", "prefetch_tiles", tInit.mPrefetchTiles);
    }
    prime::MapLibrary::Get().Init(tInit);

    OpenMainMap();
    CreateCommands();
    CreateMenu();
    DebugLine("loaded (version " + std::string(kVersion) + ", API " + prime::VersionString() + ")");
}

void PrimeMapPlugin::Stop()
{
    for (XPLMCommandRef tCommand : {mToggleCommand, mControlsCommand, mDiagnosticsCommand, mNextModeCommand,
                                    mPrecacheCommand, mCancelPrecacheCommand, mSecondMapCommand})
    {
        XPLMUnregisterCommandHandler(tCommand, CommandCallback, 1, this);
    }
    // Haritalar birakilinca ayarlari kutuphane tarafindan yazilir ve pencereleri kapanir
    mSecondMap.reset();
    mMap.reset();
    prime::MapLibrary::Get().Shutdown();
    if (mMenu)
    {
        XPLMDestroyMenu(mMenu);
        mMenu = nullptr;
    }
}

void PrimeMapPlugin::MigrateLegacyFiles() const
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
    DebugLine("settings of an older version copied to " + tNewDirectory.string());
}

void PrimeMapPlugin::OpenMainMap()
{
    // Fabrika: once nesne, sonra ayarlar, en son Open. Burada verilenler ILK acilisin
    // varsayilanlaridir; maps/main/map_settings.ini varsa Open onu okur (kullanicinin son hali).
    mMap = prime::MapLibrary::Get().CreateMap();
    if (!mMap)
    {
        return;
    }
    mMap->SetName(kMainMapName);
    mMap->SetStyle(prime::WindowStyle::Standalone);
    mMap->SetTitle("Prime Map");
    mMap->SetUiVisible(true);
    mMap->SetMouseInput(true);
    mMap->SetVisible(false);  // ilk kurulumda gizli: menuden / komutla acilir
    mMap->Open();             // yer verilmedi: kutuphane ekranin ortasina, %70 boyutunda acar
}

void PrimeMapPlugin::ToggleSecondMap()
{
    // Coklu harita ornegi: ikinci harita ilkinden bagimsizdir (gorunum, mod, katmanlar, cizimler,
    // zemin ayarlari, dosyalar); onbellek ortaktir. Kapatilinca nesne birakilir (ayarlari yazilir).
    if (mSecondMap)
    {
        mSecondMap.reset();
        return;
    }
    mSecondMap = prime::MapLibrary::Get().CreateMap();
    if (!mSecondMap)
    {
        return;
    }
    mSecondMap->SetName(kSecondMapName);
    mSecondMap->SetStyle(prime::WindowStyle::Standalone);
    mSecondMap->SetTitle("Prime Map 2");
    mSecondMap->SetUiVisible(true);
    mSecondMap->SetMouseInput(true);
    mSecondMap->SetMode(prime::MapMode::Vfr);  // ilk acilista ana haritadan farkli gorunsun
    if (mMap)
    {
        // Ilk acilista ana haritanin yaninda, yari boyutta (sonraki acilislarda dosyadaki yer)
        const prime::Geometry tMain = mMap->GetGeometry();
        mSecondMap->SetGeometry({tMain.mLeft + 40, tMain.mTop - 40, tMain.mWidth * 2 / 3, tMain.mHeight * 2 / 3});
    }
    mSecondMap->Open();
    mSecondMap->SetVisible(true);  // dosyada gizli kaydedilmis olsa da menuden acilinca gorunsun
}

void PrimeMapPlugin::CreateCommands()
{
    // Kullanici bu komutlari Settings -> Keyboard'da "prime/map" ile arayip tusa atayabilir
    mToggleCommand = XPLMCreateCommand("prime/map/toggle", "Map: show / hide the map window");
    mControlsCommand = XPLMCreateCommand("prime/map/controls", "Map: show / hide the controls");
    mDiagnosticsCommand = XPLMCreateCommand("prime/map/diagnostics", "Map: toggle diagnostics mode");
    mNextModeCommand = XPLMCreateCommand("prime/map/next_mode", "Map: next map mode (Scenery / VFR / IFR)");
    mPrecacheCommand = XPLMCreateCommand("prime/map/precache_view", "Map: pre-cache terrain of the visible area");
    mCancelPrecacheCommand = XPLMCreateCommand("prime/map/precache_cancel", "Map: cancel terrain pre-cache");
    mSecondMapCommand = XPLMCreateCommand("prime/map/second_map", "Map: open / close the second map window");
    for (XPLMCommandRef tCommand : {mToggleCommand, mControlsCommand, mDiagnosticsCommand, mNextModeCommand,
                                    mPrecacheCommand, mCancelPrecacheCommand, mSecondMapCommand})
    {
        XPLMRegisterCommandHandler(tCommand, CommandCallback, 1, this);
    }
}

void PrimeMapPlugin::CreateMenu()
{
    // Plugins > Prime Map Plugin
    XPLMMenuID tPluginsMenu = XPLMFindPluginsMenu();
    const int tSlot = XPLMAppendMenuItem(tPluginsMenu, "Prime Map Plugin", nullptr, 0);
    mMenu = XPLMCreateMenu("Prime Map Plugin", tPluginsMenu, tSlot, MenuCallback, this);
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

void PrimeMapPlugin::ToggleDiagnostics()
{
    if (!mMap)
    {
        return;
    }
    mMap->SetDiagnostics(!mMap->IsDiagnosticsOn());
    if (mMenu && mDiagnosticsItem >= 0)
    {
        XPLMCheckMenuItem(mMenu, mDiagnosticsItem, mMap->IsDiagnosticsOn() ? xplm_Menu_Checked : xplm_Menu_Unchecked);
    }
}

std::string PrimeMapPlugin::FindPluginDirectory()
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

void PrimeMapPlugin::MenuCallback(void* pMenuRef, void* pItemRef)
{
    static_cast<PrimeMapPlugin*>(pMenuRef)->OnMenu(static_cast<int>(reinterpret_cast<intptr_t>(pItemRef)));
}

int PrimeMapPlugin::CommandCallback(XPLMCommandRef pCommand, XPLMCommandPhase pPhase, void* pRefcon)
{
    return static_cast<PrimeMapPlugin*>(pRefcon)->OnCommand(pCommand, pPhase);
}

void PrimeMapPlugin::OnMenu(int pItem)
{
    if (pItem == kMenuDiagnostics)
    {
        ToggleDiagnostics();
    }
    else if (pItem == kMenuClearTrack && mMap)
    {
        mMap->ClearTrack();
    }
}

int PrimeMapPlugin::OnCommand(XPLMCommandRef pCommand, XPLMCommandPhase pPhase)
{
    if (pPhase != xplm_CommandBegin || !mMap)
    {
        return 0;
    }
    if (pCommand == mToggleCommand)
    {
        mMap->SetVisible(!mMap->IsVisible());
    }
    else if (pCommand == mControlsCommand)
    {
        mMap->SetUiVisible(!mMap->IsUiVisible());
    }
    else if (pCommand == mDiagnosticsCommand)
    {
        ToggleDiagnostics();
    }
    else if (pCommand == mNextModeCommand)
    {
        mMap->NextMode();
    }
    else if (pCommand == mPrecacheCommand)
    {
        mMap->PrecacheVisibleArea();
    }
    else if (pCommand == mCancelPrecacheCommand)
    {
        mMap->CancelPrecache();
    }
    else if (pCommand == mSecondMapCommand)
    {
        ToggleSecondMap();
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
    std::strcpy(pOutName, "Prime Map Plugin");
    std::strcpy(pOutSignature, "prime.prime_map_plugin");
    std::strcpy(pOutDescription, "Layered moving map in its own window (R&D)");
    gPlugin = std::make_unique<PrimeMapPlugin>();
    gPlugin->Start();
    return 1;
}

PLUGIN_API void XPluginStop()
{
    if (gPlugin)
    {
        gPlugin->Stop();
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
