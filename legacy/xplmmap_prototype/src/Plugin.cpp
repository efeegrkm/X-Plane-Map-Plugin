// Plugin.cpp
// X-Plane'in cagirdigi 5 giris fonksiyonu + menu, komutlar ve her karede
// calisan "flight loop".
//
// Dosyalarin gorevleri:
//   Plugin.cpp      : giris noktalari, menu, komutlar, flight loop
//   MapLayer.cpp    : X-Plane haritasina eklenen katman (cizim, ikon, etiket)
//   Overlay.cpp     : tiklama penceresi + paneli haritayla senkron tutar
//   ClickWindow.cpp : haritanin ustundeki gorunmez tiklama penceresi
//   ButtonPanel.cpp : buton paneli
//   Drawings.cpp    : kullanicinin cizdikleri (nokta, rota, bolge)
//   ScreenMath.cpp  : koordinat donusum matematigi
//   ScreenState.cpp : boxel <-> piksel donusumunun yakalanmasi
//   Geo.cpp         : mesafe, buyuk daire, ucgenleme

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "XPLMDataAccess.h"
#include "XPLMDefs.h"
#include "XPLMMenus.h"
#include "XPLMNavigation.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#include "Drawings.h"
#include "Log.h"
#include "MapLayer.h"
#include "Overlay.h"

namespace {

// --- Menu ------------------------------------------------------------------------
enum MenuItem { kMenuToggleMap = 1, kMenuDiagnostics, kMenuClearDrawings, kMenuClearTrail };

XPLMMenuID g_menu = nullptr;
int g_diagItemIndex = -1;
XPLMCommandRef g_mapToggleCmd = nullptr;

// --- Komutlar (X-Plane klavye ayarlarindan tusa atanabilir) ---------------------
XPLMCommandRef g_cmdPointAtCursor = nullptr;
XPLMCommandRef g_cmdFinish = nullptr;
XPLMCommandRef g_cmdPan = nullptr;
XPLMCommandRef g_cmdDiag = nullptr;

// --- Veri -------------------------------------------------------------------------
XPLMDataRef g_drLat = nullptr;
XPLMDataRef g_drLon = nullptr;
float g_lastTrailTime = -10.0f;
bool g_demoDone = false;

void ToggleDiagnostics() {
    bool on = !Overlay::Diagnostics();
    Overlay::SetDiagnostics(on);
    if (g_menu && g_diagItemIndex >= 0) {
        XPLMCheckMenuItem(g_menu, g_diagItemIndex, on ? xplm_Menu_Checked : xplm_Menu_Unchecked);
    }
}

void MenuHandler(void*, void* itemRef) {
    switch (static_cast<int>(reinterpret_cast<intptr_t>(itemRef))) {
        case kMenuToggleMap:
            if (g_mapToggleCmd) {
                XPLMCommandOnce(g_mapToggleCmd);
            } else {
                Log("harita komutu bulunamadi; haritayi X-Plane'in kendi tusuyla acin");
            }
            break;
        case kMenuDiagnostics: ToggleDiagnostics(); break;
        case kMenuClearDrawings: Drawings::Clear(); break;
        case kMenuClearTrail: MapLayer::ClearTrail(); break;
    }
}

int CommandHandler(XPLMCommandRef cmd, XPLMCommandPhase phase, void*) {
    if (phase != xplm_CommandBegin) return 0;
    if (cmd == g_cmdPointAtCursor) {
        // Yedek yol: tiklama penceresi calismazsa imlecin altina tusla ekle
        LatLon p;
        if (Drawings::GetMode() == Drawings::Mode::Pan) {
            Log("once panelden Nokta/Rota/Bolge modunu secin");
        } else if (MapLayer::GetHover(&p)) {
            Drawings::AddVertex(p);
        } else {
            Log("imlec haritanin uzerinde degil");
        }
    } else if (cmd == g_cmdFinish) {
        Drawings::Finish();
    } else if (cmd == g_cmdPan) {
        Drawings::SetMode(Drawings::Mode::Pan);
    } else if (cmd == g_cmdDiag) {
        ToggleDiagnostics();
    }
    return 0;
}

// X-Plane'in yuklu nav verisinde havalimani ara (dosya okumadan)
bool FindAirport(const char* icao, LatLon* out) {
    char idBuf[16];
    std::snprintf(idBuf, sizeof(idBuf), "%s", icao);  // SDK surumleri arasi uyum icin degistirilebilir kopya
    XPLMNavRef ref = XPLMFindNavAid(nullptr, idBuf, nullptr, nullptr, nullptr, xplm_Nav_Airport);
    if (ref == XPLM_NAV_NOT_FOUND) return false;
    float lat = 0, lon = 0;
    char id[32] = "";
    XPLMGetNavAidInfo(ref, nullptr, &lat, &lon, nullptr, nullptr, nullptr, id, nullptr, nullptr);
    if (std::strcmp(id, icao) != 0) return false;  // sadece tam eslesme
    out->lat = lat;
    out->lon = lon;
    return true;
}

void SetupDemoRoute() {
    LatLon a, b;
    bool okA = FindAirport("LTFM", &a);
    bool okB = FindAirport("LTAC", &b);
    if (!okA) a = {41.2620, 28.7420};
    if (!okB) b = {40.1281, 32.9951};
    Log("demo rota: LTFM %s, LTAC %s", okA ? "nav verisinden bulundu" : "BULUNAMADI (sabit koordinat)",
        okB ? "nav verisinden bulundu" : "BULUNAMADI (sabit koordinat)");
    MapLayer::SetDemoRoute(a, "LTFM", b, "LTAC");
}

float FlightLoop(float, float, int, void*) {
    const float now = XPLMGetElapsedTime();

    Overlay::Update();

    // Ucak izi: saniyede bir konum al
    if (now - g_lastTrailTime >= 1.0f) {
        g_lastTrailTime = now;
        LatLon p = {XPLMGetDatad(g_drLat), XPLMGetDatad(g_drLon)};
        MapLayer::AddTrailPoint(p);
    }

    // Demo rota: nav verisi yuklendikten sonra bir kez
    if (!g_demoDone && now > 3.0f) {
        g_demoDone = true;
        SetupDemoRoute();
    }

    return -1.0f;  // her karede tekrar cagir
}

// Ikon PNG'sinin yolunu bul: <plugin klasoru>/harita_ikon.png
// X-Plane ikon yolunu X-Plane klasorune gore istedigi icin goreceli yola ceviriyoruz.
void FindIconPath(std::string* relative, bool* exists) {
    char xpl[2048] = "";
    XPLMGetPluginInfo(XPLMGetMyID(), nullptr, xpl, nullptr, nullptr);  // .../HaritaPlugin/64/win.xpl
    std::string path = xpl;
    for (int i = 0; i < 2; ++i) {  // dosya adini ve "64" klasorunu at
        size_t cut = path.find_last_of("/\\");
        if (cut != std::string::npos) path.erase(cut);
    }
    std::string absolute = path + "/harita_ikon.png";

    FILE* f = std::fopen(absolute.c_str(), "rb");
    *exists = (f != nullptr);
    if (f) std::fclose(f);

    char sys[2048] = "";
    XPLMGetSystemPath(sys);  // X-Plane klasoru (sonunda ayrac ile)
    std::string rel = absolute;
    if (std::strlen(sys) > 0 && rel.compare(0, std::strlen(sys), sys) == 0) rel.erase(0, std::strlen(sys));
    for (char& c : rel) {
        if (c == '\\') c = '/';
    }
    *relative = rel;
    Log("ikon: %s (%s)", rel.c_str(), *exists ? "bulundu" : "BULUNAMADI, noktalar ikonsuz cizilecek");
}

}  // namespace

// ============================================================================
// X-Plane'in cagirdigi giris noktalari
// ============================================================================

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
    std::strcpy(outName, "Harita Plugin");
    std::strcpy(outSig, "ozdev.harita.plugin");
    std::strcpy(outDesc, "X-Plane haritasi uzerinde cizim ve isaretleme (AR-GE)");

    // Dosya yollarini isletim sisteminin kendi bicimiyle al
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);

    g_drLat = XPLMFindDataRef("sim/flightmodel/position/latitude");
    g_drLon = XPLMFindDataRef("sim/flightmodel/position/longitude");

    // X-Plane'in hazir harita ac/kapa komutu
    g_mapToggleCmd = XPLMFindCommand("sim/map/show_current");
    if (!g_mapToggleCmd) g_mapToggleCmd = XPLMFindCommand("sim/map/show_toggle");

    // Menu: Plugins > Harita Plugin
    XPLMMenuID plugins = XPLMFindPluginsMenu();
    int slot = XPLMAppendMenuItem(plugins, "Harita Plugin", nullptr, 0);
    g_menu = XPLMCreateMenu("Harita Plugin", plugins, slot, MenuHandler, nullptr);
    XPLMAppendMenuItem(g_menu, "Haritayi ac / kapat", reinterpret_cast<void*>(intptr_t(kMenuToggleMap)), 0);
    g_diagItemIndex =
        XPLMAppendMenuItem(g_menu, "Teshis modu", reinterpret_cast<void*>(intptr_t(kMenuDiagnostics)), 0);
    XPLMCheckMenuItem(g_menu, g_diagItemIndex, xplm_Menu_Unchecked);
    XPLMAppendMenuItem(g_menu, "Cizimleri temizle", reinterpret_cast<void*>(intptr_t(kMenuClearDrawings)), 0);
    XPLMAppendMenuItem(g_menu, "Ucak izini temizle", reinterpret_cast<void*>(intptr_t(kMenuClearTrail)), 0);

    // Komutlar
    g_cmdPointAtCursor = XPLMCreateCommand("ozdev/harita/imlece_nokta", "Harita: imlecin altina nokta/kose ekle");
    g_cmdFinish = XPLMCreateCommand("ozdev/harita/bitir", "Harita: rota/bolgeyi bitir");
    g_cmdPan = XPLMCreateCommand("ozdev/harita/gezin", "Harita: isaretlemeyi kapat (gezin)");
    g_cmdDiag = XPLMCreateCommand("ozdev/harita/teshis", "Harita: teshis modunu ac/kapat");
    XPLMRegisterCommandHandler(g_cmdPointAtCursor, CommandHandler, 1, nullptr);
    XPLMRegisterCommandHandler(g_cmdFinish, CommandHandler, 1, nullptr);
    XPLMRegisterCommandHandler(g_cmdPan, CommandHandler, 1, nullptr);
    XPLMRegisterCommandHandler(g_cmdDiag, CommandHandler, 1, nullptr);

    std::string icon;
    bool iconExists = false;
    FindIconPath(&icon, &iconExists);

    MapLayer::Init(icon, iconExists);
    Overlay::Init();

    XPLMRegisterFlightLoopCallback(FlightLoop, -1.0f, nullptr);

    Log("yuklendi (surum 0.3)");
    return 1;
}

PLUGIN_API void XPluginStop() {
    XPLMUnregisterFlightLoopCallback(FlightLoop, nullptr);
    XPLMUnregisterCommandHandler(g_cmdPointAtCursor, CommandHandler, 1, nullptr);
    XPLMUnregisterCommandHandler(g_cmdFinish, CommandHandler, 1, nullptr);
    XPLMUnregisterCommandHandler(g_cmdPan, CommandHandler, 1, nullptr);
    XPLMUnregisterCommandHandler(g_cmdDiag, CommandHandler, 1, nullptr);
    Overlay::Shutdown();
    MapLayer::Shutdown();
    if (g_menu) XPLMDestroyMenu(g_menu);
    g_menu = nullptr;
    Log("kapandi");
}

PLUGIN_API int XPluginEnable() { return 1; }

PLUGIN_API void XPluginDisable() {}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID, int, void*) {}
