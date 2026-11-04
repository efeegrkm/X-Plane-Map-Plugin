// Plugin.cpp
// X-Plane'in cagirdigi 5 giris fonksiyonu + menu, komutlar ve her karede
// calisan "flight loop".
//
// Plugin'in tum parcalari MapPlugin sinifinin icinde yasar; X-Plane'in
// giris fonksiyonlari sadece bu sinifi olusturup yok eder. XPLM callback'lerine
// "refcon" olarak nesnenin adresi verilir, boylece global degisken gerekmez
// (tek istisna: giris fonksiyonlarinin eristigi gPlugin).
//
// Katman sirasi (alttan uste) CreateLayers() icinde belirlenir.

#include <array>
#include <cstdint>
#include <format>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "XPLMDefs.h"
#include "XPLMMenus.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#include "Drawings.h"
#include "LayerManager.h"
#include "Log.h"
#include "MapTheme.h"
#include "MapWindow.h"
#include "Settings.h"
#include "data/AircraftData.h"
#include "data/AirwayDatabase.h"
#include "data/AirportGeometryStore.h"
#include "data/AirportDetailStore.h"
#include "data/AptDatIndexer.h"
#include "data/NavDatabase.h"
#include "data/NavLoader.h"
#include "data/TerrainClasses.h"
#include "data/TerrainStore.h"
#include "layers/AircraftLayer.h"
#include "layers/AirportLayer.h"
#include "layers/AirportObjectsLayer.h"
#include "layers/TaxiRouteLayer.h"
#include "layers/AirportSurfaceLayer.h"
#include "layers/AirwayLayer.h"
#include "layers/DrawingLayer.h"
#include "layers/FmsLayer.h"
#include "layers/GridLayer.h"
#include "layers/NavaidLayer.h"
#include "layers/RunwayLayer.h"
#include "layers/TerrainLayer.h"
#include "layers/TrackLayer.h"

namespace
{

const char* kVersion = "0.13";
const char* kSettingsFileName = "map_settings.ini";
const char* kDrawingsFileName = "map_drawings.txt";
const char* kTerrainColorsFileName = "terrain_colors.ini";  // kullanici zemin renklerini buradan degistirir
const char* kTerrainCacheFolder = "cache/terrain/";
const char* kAirportCacheFolder = "cache/airports/";
const double kNavLoadBudgetMs = 4.0;  // nav verisi okunurken her karede en fazla bu kadar

// Havalimani ozetlerinin kaynagi: apt.dat dosyalarini tarayan indeksleyici
std::unique_ptr<IAirportGeometrySource> CreateAirportGeometrySource()
{
    return std::make_unique<AptDatIndexer>();
}

class MapPlugin
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
    void StepLoading();
    static void LogReport(const std::string& pReport);
    void LoadTerrainColors();

    void CreateLayers();
    void CreateCommands();
    void CreateMenu();
    void ToggleDiagnostics();
    static std::string FindPluginDirectory();

    Settings mSettings;
    AircraftData mAircraft;
    NavDatabase mNavDatabase;
    NavLoader mNavLoader{mNavDatabase};  // mNavDatabase'den sonra tanimli olmali
    AirportGeometryStore mAirportGeometry{CreateAirportGeometrySource()};
    AirportDetailStore mAirportDetails{mAirportGeometry};  // mAirportGeometry'den sonra tanimli olmali
    bool mAirportLoadStarted = false;
    AirwayDatabase mAirways;  // nav verisi hazir olunca arka planda okunur
    TerrainPalette mTerrainPalette;
    TerrainStore mTerrainStore;  // zemin: DSF'lerden arka planda uretilir
    std::string mPluginDirectory;
    std::string mXPlaneDirectory;
    Drawings mDrawings;
    LayerManager mLayers;  // katmanlar yukaridaki verilere bagli: onlardan SONRA tanimli olmali
    TrackLayer* mTrackLayer = nullptr;  // sahibi mLayers; menuden temizlemek icin
    std::unique_ptr<MapWindow> mWindow;

    XPLMMenuID mMenu = nullptr;
    int mDiagnosticsItem = -1;
    bool mDiagnostics = false;

    XPLMCommandRef mToggleCommand = nullptr;
    XPLMCommandRef mControlsCommand = nullptr;
    XPLMCommandRef mDiagnosticsCommand = nullptr;
    XPLMCommandRef mNextModeCommand = nullptr;
};

void MapPlugin::Start()
{
    // Dosya yollarini isletim sisteminin kendi bicimiyle al (C:\...)
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);

    mPluginDirectory = FindPluginDirectory();
    const std::string tPluginDirectory = mPluginDirectory;
    const std::string tSettingsPath = tPluginDirectory + kSettingsFileName;
    if (mSettings.Load(tSettingsPath))
    {
        Log("settings loaded: {}", tSettingsPath);
    }
    else
    {
        Log("no settings file yet, using defaults: {}", tSettingsPath);
    }

    mAircraft.Init();
    LoadTerrainColors();
    CreateLayers();

    mWindow = std::make_unique<MapWindow>(mLayers, mAircraft, mSettings, mDrawings,
                                          tPluginDirectory + kDrawingsFileName);
    mWindow->Create();

    CreateCommands();
    CreateMenu();

    XPLMRegisterFlightLoopCallback(FlightLoopCallback, -1.0f, this);
    Log("loaded (version {})", kVersion);
}

void MapPlugin::Stop()
{
    XPLMUnregisterFlightLoopCallback(FlightLoopCallback, this);
    mTerrainStore.Stop();  // zemin iscisi: o an uretilen karoyu bitirip durur
    mAirportDetails.Stop();
    XPLMUnregisterCommandHandler(mToggleCommand, CommandCallback, 1, this);
    XPLMUnregisterCommandHandler(mControlsCommand, CommandCallback, 1, this);
    XPLMUnregisterCommandHandler(mDiagnosticsCommand, CommandCallback, 1, this);
    XPLMUnregisterCommandHandler(mNextModeCommand, CommandCallback, 1, this);

    if (mWindow)
    {
        mWindow->SaveSettings();
        mWindow->Destroy();
        mWindow.reset();
    }
    if (mMenu)
    {
        XPLMDestroyMenu(mMenu);
        mMenu = nullptr;
    }
    Log("stopped");
}

void MapPlugin::CreateLayers()
{
    // Alttan uste cizim sirasi (GOREV.md Bolum 5). Etiketler de bu sirayla
    // yer bulur: ustteki satirdaki katmanin etiketi onceliklidir (FMS ve
    // kullanici cizimleri etiketlerinin yerini onceden ayirir).
    mLayers.Add(std::make_unique<TerrainLayer>(mTerrainStore, mTerrainPalette));  // en altta: zemin
    mLayers.Add(std::make_unique<AirportSurfaceLayer>(mAirportGeometry, mAirportDetails));
    mLayers.Add(std::make_unique<GridLayer>());
    // ILS tuyleri pistlerin ALTINDA: buyuk yari saydam alanlar pistleri ortmesin
    mLayers.Add(std::make_unique<NavaidLayer>(NavaidType::Ils, mNavDatabase, &mAirportGeometry));
    mLayers.Add(std::make_unique<RunwayLayer>(mAirportGeometry));
    mLayers.Add(std::make_unique<TaxiRouteLayer>(mAirportGeometry, mAirportDetails));  // varsayilan kapali
    mLayers.Add(std::make_unique<AirportObjectsLayer>(mAirportGeometry, mAirportDetails));  // pistlerin ustunde
    // Hava yollari navaid sembollerinin altinda (cizgiler sembolleri ortmesin)
    mLayers.Add(std::make_unique<AirwayLayer>(mAirways, false));
    mLayers.Add(std::make_unique<AirwayLayer>(mAirways, true));
    mLayers.Add(std::make_unique<AirportLayer>(mNavDatabase, &mAirportGeometry));
    mLayers.Add(std::make_unique<NavaidLayer>(NavaidType::Vor, mNavDatabase));
    mLayers.Add(std::make_unique<NavaidLayer>(NavaidType::Ndb, mNavDatabase));
    mLayers.Add(std::make_unique<NavaidLayer>(NavaidType::Fix, mNavDatabase));
    mLayers.Add(std::make_unique<FmsLayer>());
    std::unique_ptr<TrackLayer> tTrack = std::make_unique<TrackLayer>(mAircraft);
    mTrackLayer = tTrack.get();
    mLayers.Add(std::move(tTrack));
    mLayers.Add(std::make_unique<AircraftLayer>(mAircraft));
    mLayers.Add(std::make_unique<DrawingLayer>(mDrawings));
}

void MapPlugin::CreateCommands()
{
    // Kullanici bu komutlari Settings -> Keyboard'da "ozdev/map" ile arayip tusa atayabilir
    mToggleCommand = XPLMCreateCommand("ozdev/map/toggle", "Map: show / hide the map window");
    mControlsCommand = XPLMCreateCommand("ozdev/map/controls", "Map: show / hide the controls");
    mDiagnosticsCommand = XPLMCreateCommand("ozdev/map/diagnostics", "Map: toggle diagnostics mode");
    XPLMRegisterCommandHandler(mToggleCommand, CommandCallback, 1, this);
    XPLMRegisterCommandHandler(mControlsCommand, CommandCallback, 1, this);
    XPLMRegisterCommandHandler(mDiagnosticsCommand, CommandCallback, 1, this);
    mNextModeCommand = XPLMCreateCommand("ozdev/map/next_mode", "Map: next map mode (Scenery / VFR / IFR)");
    XPLMRegisterCommandHandler(mNextModeCommand, CommandCallback, 1, this);
}

void MapPlugin::CreateMenu()
{
    // Plugins > Map Plugin
    XPLMMenuID tPluginsMenu = XPLMFindPluginsMenu();
    const int tSlot = XPLMAppendMenuItem(tPluginsMenu, "Map Plugin", nullptr, 0);
    mMenu = XPLMCreateMenu("Map Plugin", tPluginsMenu, tSlot, MenuCallback, this);

    // Komuta bagli menu ogeleri: kullanici tus atarsa menude de gorunur
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
}

void MapPlugin::ToggleDiagnostics()
{
    mDiagnostics = !mDiagnostics;
    if (mWindow)
    {
        mWindow->SetDiagnostics(mDiagnostics);
    }
    if (mMenu && mDiagnosticsItem >= 0)
    {
        XPLMCheckMenuItem(mMenu, mDiagnosticsItem, mDiagnostics ? xplm_Menu_Checked : xplm_Menu_Unchecked);
    }
    Log("diagnostics mode: {} (DIAG lines in Log.txt)", mDiagnostics ? "on" : "off");
}

std::string MapPlugin::FindPluginDirectory()
{
    // .../Resources/plugins/MapPlugin/64/win.xpl -> .../Resources/plugins/MapPlugin/
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

void MapPlugin::MenuCallback(void* pMenuRef, void* pItemRef)
{
    static_cast<MapPlugin*>(pMenuRef)->OnMenu(static_cast<int>(reinterpret_cast<intptr_t>(pItemRef)));
}

int MapPlugin::CommandCallback(XPLMCommandRef pCommand, XPLMCommandPhase pPhase, void* pRefcon)
{
    return static_cast<MapPlugin*>(pRefcon)->OnCommand(pCommand, pPhase);
}

float MapPlugin::FlightLoopCallback(float /*pSinceLastCall*/, float /*pSinceLastLoop*/, int /*pCounter*/,
                                       void* pRefcon)
{
    return static_cast<MapPlugin*>(pRefcon)->OnFlightLoop();
}

void MapPlugin::OnMenu(int pItem)
{
    if (pItem == kMenuDiagnostics)
    {
        ToggleDiagnostics();
    }
    else if (pItem == kMenuClearTrack && mTrackLayer)
    {
        mTrackLayer->Clear();
        Log("flight track cleared");
    }
}

int MapPlugin::OnCommand(XPLMCommandRef pCommand, XPLMCommandPhase pPhase)
{
    if (pPhase != xplm_CommandBegin || !mWindow)
    {
        return 0;
    }
    if (pCommand == mToggleCommand)
    {
        mWindow->ToggleVisible();
    }
    else if (pCommand == mControlsCommand)
    {
        mWindow->ToggleControls();
    }
    else if (pCommand == mDiagnosticsCommand)
    {
        ToggleDiagnostics();
    }
    else if (pCommand == mNextModeCommand)
    {
        mWindow->NextMapMode();
    }
    return 0;  // komut bizim; baska kimse islemesin
}

float MapPlugin::OnFlightLoop()
{
    const float tNow = XPLMGetElapsedTime();
    StepLoading();
    mAircraft.Refresh();
    mLayers.UpdateAll(tNow);
    if (mWindow)
    {
        mWindow->Update(tNow);
    }
    return -1.0f;  // her karede tekrar cagir
}

void MapPlugin::StepLoading()
{
    // Pist verisi: ilk flight loop'ta (X-Plane hazir) arka planda okumaya basla
    if (!mAirportLoadStarted)
    {
        mAirportLoadStarted = true;
        std::array<char, 2048> tSystemPath = {};
        XPLMGetSystemPath(tSystemPath.data());  // "C:\X-Plane 11\" (sonunda ayirici ile)
        mXPlaneDirectory = tSystemPath.data();
        Log("indexing airports in the background: {}", mAirportGeometry.SourceName());
        mAirportGeometry.StartLoading(mXPlaneDirectory, mPluginDirectory + kAirportCacheFolder);
        mTerrainStore.Start(mXPlaneDirectory, mPluginDirectory + kTerrainCacheFolder, mTerrainPalette,
                            AllTerrainStyles());
    }
    LogReport(mTerrainStore.TakeLogLines());
    mAirportDetails.Update();  // onceki karede istenen havalimani ayrintilari isciye
    LogReport(mAirportDetails.TakeLogLines());
    if (mAirportGeometry.Poll())
    {
        Log("airport index {}: {} airports, {} runways", mAirportGeometry.LastLoadSucceeded() ? "ready" : "FAILED",
            mAirportGeometry.Count(), mAirportGeometry.RunwayCount());
        LogReport(mAirportGeometry.LastReport());
        mAirportDetails.Start();  // ayrintilar artik dosyadaki yerlerinden okunabilir
    }

    // Nav verisi: ana thread'de, her karede birkac ms
    if (!mNavLoader.IsDone())
    {
        if (mNavLoader.Step(kNavLoadBudgetMs))
        {
            // Hava yollarinin uc noktalari nav verisinden bulunur: nav bitince basla.
            // X-Plane'in kurali: once Custom Data (guncel AIRAC), yoksa varsayilan.
            mAirways.StartLoading({mXPlaneDirectory + "Custom Data/earth_awy.dat",
                                   mXPlaneDirectory + "Resources/default data/earth_awy.dat"},
                                  mNavDatabase);
        }
    }
    if (mAirways.Poll())
    {
        Log("airways {}: {}", mAirways.IsReady() ? "ready" : "FAILED", mAirways.LastReport());
    }

    // Alt seritteki durum yazisi
    if (!mWindow)
    {
        return;
    }
    std::string tMessage;
    if (!mNavLoader.IsDone() && mNavLoader.VisitedCount() > 0)
    {
        tMessage = std::format("Loading nav data: {}", mNavLoader.VisitedCount());
    }
    if (mAirportGeometry.IsLoading())
    {
        tMessage += tMessage.empty() ? "" : "   ";
        tMessage += "Indexing airports...";
    }
    if (mAirportDetails.PendingCount() > 0)
    {
        tMessage += tMessage.empty() ? "" : "   ";
        tMessage += std::format("Loading airport details: {}", mAirportDetails.PendingCount());
    }
    if (mAirways.IsLoading())
    {
        tMessage += tMessage.empty() ? "" : "   ";
        tMessage += "Reading airways...";
    }
    const std::string tTerrainStatus = mTerrainStore.StatusText();
    if (!tTerrainStatus.empty())
    {
        tMessage += tMessage.empty() ? "" : "   ";
        tMessage += tTerrainStatus;
    }
    mWindow->SetStatusMessage(tMessage);
}

void MapPlugin::LoadTerrainColors()
{
    // Dosya yoksa varsayilan renklerle olustur: kullanici acip degistirebilsin
    Settings tColors;
    const std::string tPath = mPluginDirectory + kTerrainColorsFileName;
    if (tColors.Load(tPath))
    {
        mTerrainPalette.Load(tColors);
        Log("terrain colors loaded: {}", tPath);
        return;
    }
    mTerrainPalette.Save(tColors);
    tColors.SetComment("Map Plugin terrain colors. Edit while X-Plane is closed.\n"
                       "[colors]: <class> = R G B (0..255). [shading] strength = 0 (flat) .. 1 (strong relief).\n"
                       "Colors are applied when X-Plane starts; the cache/terrain folder does not need to be deleted.\n"
                       "Delete this file to get the default colors back.");
    tColors.Save();
    Log("terrain colors file created with defaults: {}", tPath);
}

void MapPlugin::LogReport(const std::string& pReport)
{
    // Rapor birden cok satir olabilir: her satiri ayri log satiri yap
    size_t tStart = 0;
    while (tStart < pReport.size())
    {
        size_t tEnd = pReport.find('\n', tStart);
        if (tEnd == std::string::npos)
        {
            tEnd = pReport.size();
        }
        if (tEnd > tStart)
        {
            Log("  {}", pReport.substr(tStart, tEnd - tStart));
        }
        tStart = tEnd + 1;
    }
}

std::unique_ptr<MapPlugin> gPlugin;

}  // namespace

// ============================================================================
// X-Plane'in cagirdigi giris noktalari
// ============================================================================

PLUGIN_API int XPluginStart(char* pOutName, char* pOutSignature, char* pOutDescription)
{
    std::strcpy(pOutName, "Map Plugin");
    std::strcpy(pOutSignature, "ozdev.map.plugin");
    std::strcpy(pOutDescription, "Layered moving map in its own window (R&D)");

    gPlugin = std::make_unique<MapPlugin>();
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
