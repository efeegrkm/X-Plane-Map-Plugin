// Overlay.cpp
#include "Overlay.h"

#include <cmath>
#include <cstdio>

#include "XPLMDisplay.h"
#include "XPLMProcessing.h"

#include "ButtonPanel.h"
#include "ClickWindow.h"
#include "Drawings.h"
#include "Log.h"
#include "MapLayer.h"
#include "ScreenState.h"

namespace Overlay {

namespace {
bool g_diag = false;
float g_lastUpdate = -1.0f;
float g_lastDiagLog = -1.0f;
bool g_panelInsideMap = false;
char g_status[64] = "";

void SetStatus(const char* s) { std::snprintf(g_status, sizeof(g_status), "%s", s); }

// Haritanin ekrandaki dikdortgenini boxel olarak hesapla
bool ComputeMapBox(RectI* out) {
    const MapLayer::ScreenInfo& info = MapLayer::Info();
    if (!info.valid) {
        SetStatus("Harita matrisi yok");
        return false;
    }
    if (!Screen::HasWindowTransform()) {
        SetStatus("Hazirlaniyor...");  // panel ilk kez cizilince duzelir
        return false;
    }
    RectD boxels;
    if (!Screen::PixelRectToBoxel(info.mapRectPixels, &boxels)) return false;
    RectI r = RoundRect(boxels);

    // Akil kontrolu: cok kucuk veya ekranin cok disinda ise guvenme
    int sl, st, sr, sb;
    XPLMGetScreenBoundsGlobal(&sl, &st, &sr, &sb);
    const int slack = 100;
    bool sane = r.Width() >= 60 && r.Height() >= 60 && r.left >= sl - slack && r.right <= sr + slack &&
                r.bottom >= sb - slack && r.top <= st + slack;
    if (!sane) {
        SetStatus("Hizalama hatali");
        return false;
    }
    *out = r;
    return true;
}

// Paneli haritanin soluna, sigmazsa sagina, o da olmazsa icine (sol ust) koy
void PlacePanel(const RectI* map) {
    const int w = ButtonPanel::Width(), h = ButtonPanel::Height(), gap = 16;
    int sl, st, sr, sb;
    XPLMGetScreenBoundsGlobal(&sl, &st, &sr, &sb);

    RectI p;
    if (!map) {
        p.left = sl + 40;
        p.top = st - 120;
        g_panelInsideMap = false;
    } else {
        p.top = map->top;
        if (p.top - h < sb) p.top = sb + h;
        if (p.top > st) p.top = st;

        if (map->left - gap - w >= sl) {          // solda yer var
            p.left = map->left - gap - w;
            g_panelInsideMap = false;
        } else if (map->right + gap + w <= sr) {  // sagda yer var
            p.left = map->right + gap;
            g_panelInsideMap = false;
        } else {                                  // haritanin icine
            p.left = map->left + 10;
            p.top = map->top - 10;
            g_panelInsideMap = true;
        }
    }
    p.right = p.left + w;
    p.bottom = p.top - h;
    ButtonPanel::SetRect(p);
}

void LogDiagnostics(float now, bool haveBox, const RectI& box) {
    if (!g_diag || now - g_lastDiagLog < 1.0f) return;
    g_lastDiagLog = now;
    const MapLayer::ScreenInfo& i = MapLayer::Info();
    const Affine2D& m = i.mapToPixel;
    const Affine2D& w = Screen::BoxelToPixelAffine();
    int mx = 0, my = 0;
    XPLMGetMouseLocationGlobal(&mx, &my);
    Log("TESHIS harita: gecerli=%d donuk=%d scissor=%d fb=%d vp=(%d,%d,%d,%d)", i.valid, i.rotated,
        i.rectFromScissor, i.framebuffer, i.viewport[0], i.viewport[1], i.viewport[2], i.viewport[3]);
    Log("TESHIS harita->piksel: a=%.5f b=%.5f c=%.5f d=%.5f tx=%.1f ty=%.1f", m.a, m.b, m.c, m.d, m.tx, m.ty);
    Log("TESHIS boxel->piksel: a=%.4f d=%.4f tx=%.1f ty=%.1f fb=%d", w.a, w.d, w.tx, w.ty,
        Screen::WindowFramebuffer());
    Log("TESHIS harita piksel=(%.0f,%.0f,%.0f,%.0f) boxel=%s(%d,%d,%d,%d) fare=(%d,%d)",
        i.mapRectPixels.left, i.mapRectPixels.top, i.mapRectPixels.right, i.mapRectPixels.bottom,
        haveBox ? "" : "YOK ", box.left, box.top, box.right, box.bottom, mx, my);
    LatLon h;
    if (MapLayer::GetHover(&h)) Log("TESHIS imlec lat/lon: %.5f %.5f", h.lat, h.lon);
}
}  // namespace

void Init() {
    ClickWindow::Create();
    ButtonPanel::Create();
}

void Shutdown() {
    ClickWindow::Destroy();
    ButtonPanel::Destroy();
}

void Update() {
    const float now = XPLMGetElapsedTime();
    g_lastUpdate = now;

    // 1) Harita kapaliysa hepsini gizle
    if (!MapLayer::IsMapVisible()) {
        ClickWindow::SetVisible(false);
        ButtonPanel::SetVisible(false);
        return;
    }

    // 2) Panel harita acikken hep gorunur (ilk cizimde boxel donusumunu da verir)
    ButtonPanel::SetVisible(true);
    SetStatus("");

    RectI box;
    bool haveBox = ComputeMapBox(&box);
    PlacePanel(haveBox ? &box : nullptr);

    // 3) Tiklama penceresi sadece isaretleme modunda ve hizalama dogruysa
    bool marking = Drawings::GetMode() != Drawings::Mode::Pan;
    if (marking && haveBox) {
        ClickWindow::SetRect(box);
        ClickWindow::SetVisible(true);
    } else {
        ClickWindow::SetVisible(false);
    }

    // 4) On-arka sirasi: haritaya tiklaninca X-Plane haritayi one alir;
    //    gerekiyorsa bizimkileri tekrar one getir (once tiklama penceresi,
    //    sonra panel, ki panel en ustte kalsin).
    bool needFront = (marking && haveBox) || g_panelInsideMap;
    if (needFront && !XPLMIsWindowInFront(ButtonPanel::Id())) {
        if (ClickWindow::IsVisible()) XPLMBringWindowToFront(ClickWindow::Id());
        XPLMBringWindowToFront(ButtonPanel::Id());
    }

    LogDiagnostics(now, haveBox, box);
}

void OnMapDrawn() {
    // Normalde Update() flight loop'tan her karede gelir. Gelmiyorsa
    // (simulator duraklatildiginda olabilir) en azindan konumlari guncelle.
    // Cizim sirasinda pencere ac/kapa veya on-arka degisikligi yapmiyoruz.
    const float now = XPLMGetElapsedTime();
    if (g_lastUpdate >= 0 && now - g_lastUpdate < 0.3f) return;
    RectI box;
    if (!ComputeMapBox(&box)) return;
    PlacePanel(&box);
    if (ClickWindow::IsVisible()) ClickWindow::SetRect(box);
}

void HandleMapClick(int x, int y) {
    const MapLayer::ScreenInfo& info = MapLayer::Info();
    double px, py;
    if (!info.valid || !Screen::BoxelToPixel(x, y, &px, &py)) {
        Log("tiklama islenemedi: donusum yok");
        return;
    }
    Affine2D pixelToMap;
    if (!Invert(info.mapToPixel, &pixelToMap)) return;
    double mapX, mapY;
    pixelToMap.Apply(px, py, &mapX, &mapY);
    MapLayer::QueueClick(mapX, mapY);  // lat/lon'a cevirme sonraki harita ciziminde
    if (g_diag) Log("TESHIS tiklama boxel=(%d,%d) piksel=(%.1f,%.1f) harita=(%.2f,%.2f)", x, y, px, py, mapX, mapY);
}

void SetDiagnostics(bool on) {
    g_diag = on;
    MapLayer::SetDiagnostics(on);
    Log("teshis modu: %s", on ? "acik" : "kapali");
}

bool Diagnostics() { return g_diag; }

const char* StatusText() { return g_status; }

}  // namespace Overlay
