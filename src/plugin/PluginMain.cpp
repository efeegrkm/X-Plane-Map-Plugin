// PluginMain.cpp
// Prime Map Plugin'in ince kabugu (0.20): X-Plane'in cagirdigi 5 giris fonksiyonu, menu,
// ozdev/map/* komutlari ve ayar dosyalari. Haritaya SADECE genel API (api/PrimeMap.h)
// uzerinden erisir; bu yuzden eklentide yapilabilen her sey API'de de vardir.
//
//   map_settings.ini   : [window] (yer, gorunurluk), [view] show_controls, [cache], [render], [text]
//                        eklentinin; [view] mode/center/zoom/follow, [tools], [layers.*] haritanin
//                        (ExportSettings / ImportSettings). Anahtarlar 0.19 ile ayni.
//   map_drawings.txt   : ExportDrawings / ImportDrawings
//   terrain_colors.ini : ExportTerrainSettings / ImportTerrainSettings
//
// Dosyalar degisince (API'nin Get...Revision sayaclari) flight loop'ta yazilir.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "XPLMDefs.h"
#include "XPLMDisplay.h"
#include "XPLMMenus.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#include "Settings.h"
#include "api/PrimeMap.h"

namespace
{

const char* kVersion = "0.20";
const char* kSettingsFileName = "map_settings.ini";
const char* kDrawingsFileName = "map_drawings.txt";
const char* kTerrainColorsFileName = "terrain_colors.ini";
const char* kWindowSection = "window";
const char* kViewSection = "view";
const char* kCacheSection = "cache";
const char* kRenderSection = "render";
const char* kTextSection = "text";
const int kMinWidth = 400;  // pencere en kucuk boyutu (boxel)
const int kMinHeight = 260;

bool ReadFile(const std::string& pPath, std::string* pText)
{
    std::ifstream tFile(pPath);
    if (!tFile)
    {
        return false;
    }
    std::ostringstream tText;
    tText << tFile.rdbuf();
    *pText = tText.str();
    return true;
}

bool WriteFile(const std::string& pPath, const std::string& pText)
{
    std::ofstream tFile(pPath, std::ios::trunc);
    if (!tFile)
    {
        return false;
    }
    tFile << pText;
    return static_cast<bool>(tFile);
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
    static float FlightLoopCallback(float pSinceLastCall, float pSinceLastLoop, int pCounter, void* pRefcon);

    void OnMenu(int pItem);
    int OnCommand(XPLMCommandRef pCommand, XPLMCommandPhase pPhase);
    float OnFlightLoop();
    void LoadTerrainColors();
    void CreateCommands();
    void CreateMenu();
    void ToggleDiagnostics();
    void SaveSettings();
    void SaveDrawings();
    PrimeMap::Geometry WindowGeometry() const;
    static PrimeMap::Geometry DefaultGeometry();
    static bool IsGeometryUsable(const PrimeMap::Geometry& pGeometry);
    static std::string FindPluginDirectory();

    std::string mDirectory;
    Settings mSettings;  // map_settings.ini
    std::unique_ptr<PrimeMap::Map> mMap;
    PrimeMap::Geometry mFloatingGeometry;  // X-Plane icindeyken son yer (pop-out'ta degismez)
    uint64_t mSavedSettings = 0;
    uint64_t mSavedDrawings = 0;
    bool mWasVisible = false;
    bool mWasUiVisible = false;

    XPLMMenuID mMenu = nullptr;
    int mDiagnosticsItem = -1;
    XPLMCommandRef mToggleCommand = nullptr;
    XPLMCommandRef mControlsCommand = nullptr;
    XPLMCommandRef mDiagnosticsCommand = nullptr;
    XPLMCommandRef mNextModeCommand = nullptr;
    XPLMCommandRef mPrecacheCommand = nullptr;
    XPLMCommandRef mCancelPrecacheCommand = nullptr;
};

void PrimeMapPlugin::Start()
{
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);
    mDirectory = FindPluginDirectory();
    const bool tHasSettings = mSettings.Load(mDirectory + kSettingsFileName);

    PrimeMap::InitConfig tInit;
    tInit.mCacheDirectory = mDirectory + "cache/";
    tInit.mLogPrefix = "[prime-map-plugin]";  // Log.txt satirlari 0.19 ile ayni
    tInit.mTerrainCacheMb = std::max(0, mSettings.GetInt(kCacheSection, "terrain_mb", 1024));
    tInit.mPrefetchTiles = std::max(0, mSettings.GetInt(kCacheSection, "prefetch_tiles", 20));
    mSettings.SetInt(kCacheSection, "terrain_mb", tInit.mTerrainCacheMb);  // dosyada gorunsun
    mSettings.SetInt(kCacheSection, "prefetch_tiles", tInit.mPrefetchTiles);
    PrimeMap::Init(tInit);
    XPLMDebugString(tHasSettings ? "[prime-map-plugin] settings loaded\n"
                                 : "[prime-map-plugin] no settings file yet, using defaults\n");

    PrimeMap::OpenConfig tOpen;
    tOpen.mGeometry.mLeft = mSettings.GetInt(kWindowSection, "left", 0);
    tOpen.mGeometry.mTop = mSettings.GetInt(kWindowSection, "top", 0);
    tOpen.mGeometry.mWidth = mSettings.GetInt(kWindowSection, "right", 0) - tOpen.mGeometry.mLeft;
    tOpen.mGeometry.mHeight = tOpen.mGeometry.mTop - mSettings.GetInt(kWindowSection, "bottom", 0);
    if (!IsGeometryUsable(tOpen.mGeometry))
    {
        tOpen.mGeometry = DefaultGeometry();
    }
    mFloatingGeometry = tOpen.mGeometry;
    tOpen.mStyle = PrimeMap::WindowStyle::Standalone;
    tOpen.mUiVisible = mSettings.GetBool(kViewSection, "show_controls", true);
    tOpen.mMouseInput = true;
    tOpen.mVisible = mSettings.GetBool(kWindowSection, "visible", false);
    tOpen.mImageCache = mSettings.GetBool(kRenderSection, "cache", true);
    tOpen.mMaxRenderFps = mSettings.GetInt(kRenderSection, "max_fps", 15);
    tOpen.mXPlaneFont = mSettings.GetString(kTextSection, "renderer", "atlas") == "xplane";
    mSettings.SetBool(kRenderSection, "cache", tOpen.mImageCache);
    mSettings.SetInt(kRenderSection, "max_fps", tOpen.mMaxRenderFps);
    mSettings.SetString(kTextSection, "renderer", tOpen.mXPlaneFont ? "xplane" : "atlas");
    mMap = PrimeMap::Map::Open(tOpen);
    if (mMap)
    {
        LoadTerrainColors();
        mMap->ImportSettings(mSettings.ToText({}));  // mod, gorunum, palet, katmanlar
        std::string tDrawings;
        if (ReadFile(mDirectory + kDrawingsFileName, &tDrawings))
        {
            mMap->ImportDrawings(tDrawings);
        }
        mSavedSettings = mMap->GetSettingsRevision();
        mSavedDrawings = mMap->GetDrawingsRevision();
        mWasVisible = mMap->IsVisible();
        mWasUiVisible = mMap->IsUiVisible();
    }

    CreateCommands();
    CreateMenu();
    XPLMRegisterFlightLoopCallback(FlightLoopCallback, -1.0f, this);
    XPLMDebugString(("[prime-map-plugin] loaded (version " + std::string(kVersion) + ", API " +
                     PrimeMap::VersionString() + ")\n").c_str());
}

void PrimeMapPlugin::Stop()
{
    XPLMUnregisterFlightLoopCallback(FlightLoopCallback, this);
    XPLMUnregisterCommandHandler(mToggleCommand, CommandCallback, 1, this);
    XPLMUnregisterCommandHandler(mControlsCommand, CommandCallback, 1, this);
    XPLMUnregisterCommandHandler(mDiagnosticsCommand, CommandCallback, 1, this);
    XPLMUnregisterCommandHandler(mNextModeCommand, CommandCallback, 1, this);
    XPLMUnregisterCommandHandler(mPrecacheCommand, CommandCallback, 1, this);
    XPLMUnregisterCommandHandler(mCancelPrecacheCommand, CommandCallback, 1, this);
    if (mMap)
    {
        SaveSettings();
        SaveDrawings();
        mMap.reset();
    }
    PrimeMap::Shutdown();
    if (mMenu)
    {
        XPLMDestroyMenu(mMenu);
        mMenu = nullptr;
    }
}

void PrimeMapPlugin::LoadTerrainColors()
{
    // Harita acildiktan hemen sonra, ilk kareden once: zemin iscisi henuz baslamadi, ayar tam etkili.
    // Dosya yoksa ya da eski palet surumundeyse yeniden yaz: kullanici acip degistirebilsin.
    const std::string tPath = mDirectory + kTerrainColorsFileName;
    std::string tText;
    const bool tExists = ReadFile(tPath, &tText);
    if (tExists)
    {
        mMap->ImportTerrainSettings(tText);
    }
    Settings tFile;
    tFile.LoadText(tText);
    Settings tCurrent;
    const std::string tExported = mMap->ExportTerrainSettings();
    tCurrent.LoadText(tExported);
    if (!tExists || tFile.GetInt("palette", "version", 0) != tCurrent.GetInt("palette", "version", 0))
    {
        WriteFile(tPath, tExported);
        XPLMDebugString(("[prime-map-plugin] terrain colors file written: " + tPath + "\n").c_str());
    }
}

PrimeMap::Geometry PrimeMapPlugin::WindowGeometry() const
{
    return mMap && !mMap->IsPoppedOut() ? mMap->GetGeometry() : mFloatingGeometry;
}

void PrimeMapPlugin::SaveSettings()
{
    if (!mMap)
    {
        return;
    }
    mSettings.MergeText(mMap->ExportSettings());
    const PrimeMap::Geometry tGeometry = WindowGeometry();
    mSettings.SetInt(kWindowSection, "left", tGeometry.mLeft);
    mSettings.SetInt(kWindowSection, "top", tGeometry.mTop);
    mSettings.SetInt(kWindowSection, "right", tGeometry.mLeft + tGeometry.mWidth);
    mSettings.SetInt(kWindowSection, "bottom", tGeometry.mTop - tGeometry.mHeight);
    mSettings.SetBool(kWindowSection, "visible", mMap->IsVisible());
    mSettings.SetBool(kViewSection, "show_controls", mMap->IsUiVisible());
    if (!mSettings.Save())
    {
        XPLMDebugString("[prime-map-plugin] WARNING: could not write the settings file\n");
    }
    mSavedSettings = mMap->GetSettingsRevision();  // ExportSettings sayaci artirir: sonra oku
}

void PrimeMapPlugin::SaveDrawings()
{
    if (!mMap)
    {
        return;
    }
    if (!WriteFile(mDirectory + kDrawingsFileName, mMap->ExportDrawings()))
    {
        XPLMDebugString("[prime-map-plugin] WARNING: could not write the drawings file\n");
    }
    mSavedDrawings = mMap->GetDrawingsRevision();
}

PrimeMap::Geometry PrimeMapPlugin::DefaultGeometry()
{
    // Ana X-Plane penceresinin ortasinda, ekranin %70'i (en fazla 1000x700)
    int tLeft = 0;
    int tTop = 0;
    int tRight = 0;
    int tBottom = 0;
    XPLMGetScreenBoundsGlobal(&tLeft, &tTop, &tRight, &tBottom);
    PrimeMap::Geometry tGeometry;
    tGeometry.mWidth = std::clamp((tRight - tLeft) * 7 / 10, kMinWidth, std::max(kMinWidth, 1000));
    tGeometry.mHeight = std::clamp((tTop - tBottom) * 7 / 10, kMinHeight, std::max(kMinHeight, 700));
    tGeometry.mLeft = (tLeft + tRight) / 2 - tGeometry.mWidth / 2;
    tGeometry.mTop = (tTop + tBottom) / 2 + tGeometry.mHeight / 2;
    return tGeometry;
}

bool PrimeMapPlugin::IsGeometryUsable(const PrimeMap::Geometry& pGeometry)
{
    if (pGeometry.mWidth < kMinWidth || pGeometry.mHeight < kMinHeight)
    {
        return false;
    }
    // Pencerenin baslik cubugu ekranda kalmali (monitor sokulmus / cozunurluk degismis olabilir)
    int tLeft = 0;
    int tTop = 0;
    int tRight = 0;
    int tBottom = 0;
    XPLMGetScreenBoundsGlobal(&tLeft, &tTop, &tRight, &tBottom);
    const int tTitleBar = 30;
    const int tRightEdge = pGeometry.mLeft + pGeometry.mWidth;
    const bool tTitleVisible = pGeometry.mTop + tTitleBar <= tTop && pGeometry.mTop >= tBottom + 20;
    const bool tHorizontalVisible = tRightEdge >= tLeft + 100 && pGeometry.mLeft <= tRight - 100;
    return tTitleVisible && tHorizontalVisible;
}

void PrimeMapPlugin::CreateCommands()
{
    // Kullanici bu komutlari Settings -> Keyboard'da "ozdev/map" ile arayip tusa atayabilir
    mToggleCommand = XPLMCreateCommand("ozdev/map/toggle", "Map: show / hide the map window");
    mControlsCommand = XPLMCreateCommand("ozdev/map/controls", "Map: show / hide the controls");
    mDiagnosticsCommand = XPLMCreateCommand("ozdev/map/diagnostics", "Map: toggle diagnostics mode");
    mNextModeCommand = XPLMCreateCommand("ozdev/map/next_mode", "Map: next map mode (Scenery / VFR / IFR)");
    mPrecacheCommand = XPLMCreateCommand("ozdev/map/precache_view", "Map: pre-cache terrain of the visible area");
    mCancelPrecacheCommand = XPLMCreateCommand("ozdev/map/precache_cancel", "Map: cancel terrain pre-cache");
    for (XPLMCommandRef tCommand : {mToggleCommand, mControlsCommand, mDiagnosticsCommand, mNextModeCommand,
                                    mPrecacheCommand, mCancelPrecacheCommand})
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

float PrimeMapPlugin::FlightLoopCallback(float /*pSinceLastCall*/, float /*pSinceLastLoop*/, int /*pCounter*/,
                                         void* pRefcon)
{
    return static_cast<PrimeMapPlugin*>(pRefcon)->OnFlightLoop();
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
    return 0;  // komut bizim; baska kimse islemesin
}

float PrimeMapPlugin::OnFlightLoop()
{
    if (!mMap)
    {
        return -1.0f;
    }
    if (!mMap->IsPoppedOut())
    {
        mFloatingGeometry = mMap->GetGeometry();
    }
    // Harita ayari degistiyse ya da pencere / kontroller acilip kapandiysa ayar dosyasini yaz
    const bool tVisible = mMap->IsVisible();
    const bool tUiVisible = mMap->IsUiVisible();
    if (mMap->GetSettingsRevision() != mSavedSettings || tVisible != mWasVisible || tUiVisible != mWasUiVisible)
    {
        mWasVisible = tVisible;
        mWasUiVisible = tUiVisible;
        SaveSettings();
    }
    if (mMap->GetDrawingsRevision() != mSavedDrawings)
    {
        SaveDrawings();
    }
    return -1.0f;
}

std::unique_ptr<PrimeMapPlugin> gPlugin;

}  // namespace

// ============================================================================
// X-Plane'in cagirdigi giris noktalari
// ============================================================================

PLUGIN_API int XPluginStart(char* pOutName, char* pOutSignature, char* pOutDescription)
{
    std::strcpy(pOutName, "Prime Map Plugin");
    std::strcpy(pOutSignature, "ozdev.prime_map_plugin");
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
