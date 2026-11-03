// ClickWindow.cpp
#include "ClickWindow.h"

#include <cstring>

#include "XPLMGraphics.h"

#include "Drawings.h"
#include "GLInclude.h"
#include "Overlay.h"
#include "ScreenState.h"

namespace ClickWindow {

namespace {
XPLMWindowID g_window = nullptr;
RectI g_rect;
bool g_visible = false;

void OnDraw(XPLMWindowID id, void*) {
    // Pencere cizilirken OpenGL "boxel -> piksel" kurulu: bunu kaydet
    Screen::CaptureWindowTransform(Overlay::Diagnostics());

    int l, t, r, b;
    XPLMGetWindowGeometry(id, &l, &t, &r, &b);

    // Ici tamamen saydam. Sadece ince bir cerceve ciziyoruz:
    // cerceve haritanin kenarlariyla cakisiyorsa hizalama dogru demektir.
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);
    if (Overlay::Diagnostics()) {
        glColor4f(1.0f, 0.15f, 0.15f, 1.0f);
        glLineWidth(2.0f);
    } else {
        glColor4f(0.0f, 0.85f, 1.0f, 0.8f);
        glLineWidth(2.0f);
    }
    glBegin(GL_LINE_LOOP);
    glVertex2f(l + 1.0f, b + 1.0f);
    glVertex2f(r - 1.0f, b + 1.0f);
    glVertex2f(r - 1.0f, t - 1.0f);
    glVertex2f(l + 1.0f, t - 1.0f);
    glEnd();
    glLineWidth(1.0f);
}

int OnClick(XPLMWindowID, int x, int y, XPLMMouseStatus status, void*) {
    if (status == xplm_MouseDown) Overlay::HandleMapClick(x, y);
    return 1;  // tiklamayi tuket (haritaya gecirme)
}

int OnRightClick(XPLMWindowID, int, int, XPLMMouseStatus status, void*) {
    if (status == xplm_MouseDown) Drawings::Finish();
    return 1;
}

int OnWheel(XPLMWindowID, int, int, int, int, void*) {
    return 0;  // tekerlegi alttaki haritaya gecir: isaretlerken zoom calissin
}

XPLMCursorStatus OnCursor(XPLMWindowID, int, int, void*) { return xplm_CursorArrow; }

void OnKey(XPLMWindowID, char, XPLMKeyFlags, char, void*, int) {}
}  // namespace

void Create() {
    if (g_window) return;
    XPLMCreateWindow_t p;
    std::memset(&p, 0, sizeof(p));
    p.structSize = sizeof(p);
    p.left = 100;
    p.top = 400;
    p.right = 400;
    p.bottom = 100;
    p.visible = 0;
    p.drawWindowFunc = OnDraw;
    p.handleMouseClickFunc = OnClick;
    p.handleRightClickFunc = OnRightClick;
    p.handleKeyFunc = OnKey;
    p.handleCursorFunc = OnCursor;
    p.handleMouseWheelFunc = OnWheel;
    p.refcon = nullptr;
    p.decorateAsFloatingWindow = xplm_WindowDecorationNone;  // cerceve/baslik yok
    p.layer = xplm_WindowLayerFloatingWindows;               // haritayla ayni katman
    g_window = XPLMCreateWindowEx(&p);
    g_visible = false;
}

void Destroy() {
    if (g_window) XPLMDestroyWindow(g_window);
    g_window = nullptr;
}

void SetVisible(bool visible) {
    if (!g_window || visible == g_visible) return;
    XPLMSetWindowIsVisible(g_window, visible ? 1 : 0);
    g_visible = visible;
}

bool IsVisible() { return g_visible; }

void SetRect(const RectI& r) {
    if (!g_window || r == g_rect) return;
    XPLMSetWindowGeometry(g_window, r.left, r.top, r.right, r.bottom);
    g_rect = r;
}

XPLMWindowID Id() { return g_window; }

}  // namespace ClickWindow
