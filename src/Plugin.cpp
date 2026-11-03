// Plugin.cpp
// X-Plane'in cagirdigi 5 giris fonksiyonu + menu, komutlar ve her karede
// calisan "flight loop".
//
// Plugin'in tum parcalari HaritaPlugin sinifinin icinde yasar; X-Plane'in
// giris fonksiyonlari sadece bu sinifi olusturup yok eder. XPLM callback'lerine
// "refcon" olarak nesnenin adresi verilir, boylece global degisken gerekmez
// (tek istisna: giris fonksiyonlarinin eristigi gPlugin).
//
// Katman sirasi (alttan uste) CreateLayers() icinde belirlenir.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "XPLMDefs.h"
#include "XPLMMenus.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#include "LayerManager.h"
#include "Log.h"
#include "MapWindow.h"
#include "Settings.h"
#include "data/AircraftData.h"
#include "data/AirportGeometryStore.h"
#include "data/AptDatReader.h"
#include "data/NavDatabase.h"
#include "data/NavLoader.h"
#include "layers/AircraftLayer.h"
#include "layers/AirportLayer.h"
#include "layers/GridLayer.h"
#include "layers/NavaidLayer.h"
#include "layers/RunwayLayer.h"

namespace
{

const char* kVersion = "0.6";
const char* kSettingsFileName = "harita_ayarlar.ini";
const double kNavLoadBudgetMs = 4.0;  // nav verisi okunurken her karede en fazla bu kadar

// ============================ MOTOR BAGLAMA NOKTASI ============================
// Pist (havalimani geometrisi) verisinin kaynagi. Ekip apt.dat motoru
// baglanirken SADECE bu fonksiyondaki satir degisir, ornek:
//     return std::make_unique<TeamAptDatSource>();
// Ayrintili talimat: src/data/AirportGeometry.h dosyasinin basi.
std::unique_ptr<IAirportGeometrySource> CreateAirportGeometrySource()
{
    return std::make_unique<AptDatReader>();  // GECICI okuyucu
}
// ===============================================================================

class HaritaPlugin
{
public:
    void Start();
    void Stop();

private:
    enum MenuItem
    {
        kMenuDiagnostics = 1,
    };

    static void MenuCallback(void* pMenuRef, void* pItemRef);
    static int CommandCallback(XPLMCommandRef pCommand, XPLMCommandPhase pPhase, void* pRefcon);
    static float FlightLoopCallback(float pSinceLastCall, float pSinceLastLoop, int pCounter, void* pRefcon);

    void OnMenu(int pItem);
    int OnCommand(XPLMCommandRef pCommand, XPLMCommandPhase pPhase);
    float OnFlightLoop();
    void StepLoading();
    static void LogReport(const std::string& pReport);

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
    bool mAirportLoadStarted = false;
    LayerManager mLayers;  // katmanlar yukaridaki verilere bagli: onlardan SONRA tanimli olmali
    std::unique_ptr<MapWindow> mWindow;

    XPLMMenuID mMenu = nullptr;
    int mDiagnosticsItem = -1;
    bool mDiagnostics = false;

    XPLMCommandRef mToggleCommand = nullptr;
    XPLMCommandRef mControlsCommand = nullptr;
    XPLMCommandRef mDiagnosticsCommand = nullptr;
};

void HaritaPlugin::Start()
{
    // Dosya yollarini isletim sisteminin kendi bicimiyle al (C:\...)
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);

    const std::string tSettingsPath = FindPluginDirectory() + kSettingsFileName;
    if (mSettings.Load(tSettingsPath))
    {
        Log("ayarlar okundu: %s", tSettingsPath.c_str());
    }
    else
    {
        Log("ayar dosyasi yok, varsayilanlar kullanilacak: %s", tSettingsPath.c_str());
    }

    mAircraft.Init();
    CreateLayers();

    mWindow = std::make_unique<MapWindow>(mLayers, mAircraft, mSettings);
    mWindow->Create();

    CreateCommands();
    CreateMenu();

    XPLMRegisterFlightLoopCallback(FlightLoopCallback, -1.0f, this);
    Log("yuklendi (surum %s)", kVersion);
}

void HaritaPlugin::Stop()
{
    XPLMUnregisterFlightLoopCallback(FlightLoopCallback, this);
    XPLMUnregisterCommandHandler(mToggleCommand, CommandCallback, 1, this);
    XPLMUnregisterCommandHandler(mControlsCommand, CommandCallback, 1, this);
    XPLMUnregisterCommandHandler(mDiagnosticsCommand, CommandCallback, 1, this);

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
    Log("kapandi");
}

void HaritaPlugin::CreateLayers()
{
    // Alttan uste cizim sirasi (GOREV.md Bolum 5). Etiketler de bu sirayla
    // yer bulur: ustteki satirdaki katmanin etiketi onceliklidir.
    // Diger katmanlar ilgili asamalarda araya eklenecek: zemin (en alt),
    // hava yollari, FMS, ucus izi (ucaktan once), kullanici cizimleri (en ust).
    mLayers.Add(std::make_unique<GridLayer>());
    mLayers.Add(std::make_unique<RunwayLayer>(mAirportGeometry));
    mLayers.Add(std::make_unique<AirportLayer>(mNavDatabase));
    mLayers.Add(std::make_unique<NavaidLayer>(NavaidType::Vor, mNavDatabase));
    mLayers.Add(std::make_unique<NavaidLayer>(NavaidType::Ndb, mNavDatabase));
    mLayers.Add(std::make_unique<NavaidLayer>(NavaidType::Ils, mNavDatabase));
    mLayers.Add(std::make_unique<NavaidLayer>(NavaidType::Fix, mNavDatabase));
    mLayers.Add(std::make_unique<AircraftLayer>(mAircraft));
}

void HaritaPlugin::CreateCommands()
{
    // Kullanici bu komutlari Settings -> Keyboard'da "ozdev/harita" ile arayip tusa atayabilir
    mToggleCommand = XPLMCreateCommand("ozdev/harita/toggle", "Harita: pencereyi ac / kapat");
    mControlsCommand = XPLMCreateCommand("ozdev/harita/kontroller", "Harita: kontrolleri gizle / goster");
    mDiagnosticsCommand = XPLMCreateCommand("ozdev/harita/teshis", "Harita: teshis modunu ac / kapat");
    XPLMRegisterCommandHandler(mToggleCommand, CommandCallback, 1, this);
    XPLMRegisterCommandHandler(mControlsCommand, CommandCallback, 1, this);
    XPLMRegisterCommandHandler(mDiagnosticsCommand, CommandCallback, 1, this);
}

void HaritaPlugin::CreateMenu()
{
    // Plugins > Harita Plugin
    XPLMMenuID tPluginsMenu = XPLMFindPluginsMenu();
    const int tSlot = XPLMAppendMenuItem(tPluginsMenu, "Harita Plugin", nullptr, 0);
    mMenu = XPLMCreateMenu("Harita Plugin", tPluginsMenu, tSlot, MenuCallback, this);

    // Komuta bagli menu ogeleri: kullanici tus atarsa menude de gorunur
    XPLMAppendMenuItemWithCommand(mMenu, "Haritayi ac / kapat", mToggleCommand);
    XPLMAppendMenuItemWithCommand(mMenu, "Kontrolleri gizle / goster", mControlsCommand);
    XPLMAppendMenuSeparator(mMenu);
    mDiagnosticsItem =
        XPLMAppendMenuItem(mMenu, "Teshis modu", reinterpret_cast<void*>(intptr_t(kMenuDiagnostics)), 0);
    XPLMCheckMenuItem(mMenu, mDiagnosticsItem, xplm_Menu_Unchecked);
}

void HaritaPlugin::ToggleDiagnostics()
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
    Log("teshis modu: %s (Log.txt'de TESHIS satirlari)", mDiagnostics ? "acik" : "kapali");
}

std::string HaritaPlugin::FindPluginDirectory()
{
    // .../Resources/plugins/HaritaPlugin/64/win.xpl -> .../Resources/plugins/HaritaPlugin/
    char tXplPath[2048] = "";
    XPLMGetPluginInfo(XPLMGetMyID(), nullptr, tXplPath, nullptr, nullptr);
    std::string tPath = tXplPath;
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

void HaritaPlugin::MenuCallback(void* pMenuRef, void* pItemRef)
{
    static_cast<HaritaPlugin*>(pMenuRef)->OnMenu(static_cast<int>(reinterpret_cast<intptr_t>(pItemRef)));
}

int HaritaPlugin::CommandCallback(XPLMCommandRef pCommand, XPLMCommandPhase pPhase, void* pRefcon)
{
    return static_cast<HaritaPlugin*>(pRefcon)->OnCommand(pCommand, pPhase);
}

float HaritaPlugin::FlightLoopCallback(float /*pSinceLastCall*/, float /*pSinceLastLoop*/, int /*pCounter*/,
                                       void* pRefcon)
{
    return static_cast<HaritaPlugin*>(pRefcon)->OnFlightLoop();
}

void HaritaPlugin::OnMenu(int pItem)
{
    if (pItem == kMenuDiagnostics)
    {
        ToggleDiagnostics();
    }
}

int HaritaPlugin::OnCommand(XPLMCommandRef pCommand, XPLMCommandPhase pPhase)
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
    return 0;  // komut bizim; baska kimse islemesin
}

float HaritaPlugin::OnFlightLoop()
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

void HaritaPlugin::StepLoading()
{
    // Pist verisi: ilk flight loop'ta (X-Plane hazir) arka planda okumaya basla
    if (!mAirportLoadStarted)
    {
        mAirportLoadStarted = true;
        char tSystemPath[2048] = "";
        XPLMGetSystemPath(tSystemPath);  // "C:\X-Plane 11\" (sonunda ayirici ile)
        Log("pist verisi okunuyor (arka planda): %s", mAirportGeometry.SourceName());
        mAirportGeometry.StartLoading(tSystemPath);
    }
    if (mAirportGeometry.Poll())
    {
        Log("pist verisi %s: %d havalimani, %d pist", mAirportGeometry.LastLoadSucceeded() ? "hazir" : "OKUNAMADI",
            mAirportGeometry.Count(), mAirportGeometry.RunwayCount());
        LogReport(mAirportGeometry.LastReport());
    }

    // Nav verisi: ana thread'de, her karede birkac ms
    if (!mNavLoader.IsDone())
    {
        mNavLoader.Step(kNavLoadBudgetMs);
    }

    // Alt seritteki durum yazisi
    if (!mWindow)
    {
        return;
    }
    std::string tMessage;
    if (!mNavLoader.IsDone() && mNavLoader.VisitedCount() > 0)
    {
        char tText[64];
        std::snprintf(tText, sizeof(tText), "Nav verisi yukleniyor: %d", mNavLoader.VisitedCount());
        tMessage = tText;
    }
    if (mAirportGeometry.IsLoading())
    {
        tMessage += tMessage.empty() ? "" : "   ";
        tMessage += "Pist verisi okunuyor...";
    }
    mWindow->SetStatusMessage(tMessage);
}

void HaritaPlugin::LogReport(const std::string& pReport)
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
            Log("  %s", pReport.substr(tStart, tEnd - tStart).c_str());
        }
        tStart = tEnd + 1;
    }
}

std::unique_ptr<HaritaPlugin> gPlugin;

}  // namespace

// ============================================================================
// X-Plane'in cagirdigi giris noktalari
// ============================================================================

PLUGIN_API int XPluginStart(char* pOutName, char* pOutSignature, char* pOutDescription)
{
    std::strcpy(pOutName, "Harita Plugin");
    std::strcpy(pOutSignature, "ozdev.harita.plugin");
    std::strcpy(pOutDescription, "Kendi penceresinde katmanli harita (AR-GE)");

    gPlugin = std::make_unique<HaritaPlugin>();
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
