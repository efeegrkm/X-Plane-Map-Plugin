// ButtonPanel.cpp
#include "ButtonPanel.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "XPLMGraphics.h"

#include "Drawings.h"
#include "GLInclude.h"
#include "MapLayer.h"
#include "Overlay.h"
#include "ScreenState.h"

namespace ButtonPanel {

namespace {

// --- Yerlesim (boxel) ---------------------------------------------------------
//
//   +-----------------------------+
//   | HARITA PLUGIN               |  baslik bandi
//   |-----------------------------|
//   | MOD                         |
//   | [ Gezin  ] [ Nokta  ]       |  2x2 mod butonlari
//   | [ Rota   ] [ Bolge  ]       |
//   | ISLEM                       |
//   | [Bitir] [Geri al] [Temizle] |  islem butonlari
//   |-----------------------------|
//   | Mod     Nokta               |  durum
//   | Enlem   41.12345 N          |
//   | Boylam  28.12345 E          |
//   | Cizim   7                   |
//   | Sol tik: nokta ekle         |  ipucu
//   | (uyari)                     |
//   +-----------------------------+
const int kWidth = 196;
const int kPad = 10;
const int kTitleH = 26;
const int kSectionH = 18;
const int kModeH = 28;
const int kActionH = 26;
const int kGap = 6;
const int kLineH = 15;
const int kStatusLines = 6;

int PanelHeight() {
    return kTitleH + kGap + kSectionH + 2 * kModeH + kGap + kGap + kSectionH + kActionH + kGap * 2 +
           kStatusLines * kLineH + kPad;
}

enum class Action { Pan, Point, Route, Region, Finish, Undo, Clear };

struct Button {
    const char* label;
    Action action;
    RectI rect;  // global boxel
};

XPLMWindowID g_window = nullptr;
RectI g_rect;
bool g_visible = false;

// Butonlarin pencere icindeki yerleri (her cizimde/tiklamada yeniden hesaplanir)
int Layout(int l, int t, Button* out) {
    const int innerL = l + kPad, innerR = l + kWidth - kPad;
    int y = t - kTitleH - kGap - kSectionH;  // "MOD" basliginin alti

    const int modeW = (innerR - innerL - kGap) / 2;
    const char* modeLabels[4] = {"Gezin", "Nokta", "Rota", "Bolge"};
    const Action modeActions[4] = {Action::Pan, Action::Point, Action::Route, Action::Region};
    int n = 0;
    for (int i = 0; i < 4; ++i) {
        int col = i % 2, row = i / 2;
        RectI r;
        r.left = innerL + col * (modeW + kGap);
        r.right = r.left + modeW;
        r.top = y - row * (kModeH + kGap);
        r.bottom = r.top - kModeH;
        out[n++] = {modeLabels[i], modeActions[i], r};
    }
    y -= 2 * kModeH + kGap + kGap + kSectionH;  // "ISLEM" basliginin alti

    const int actW = (innerR - innerL - 2 * kGap) / 3;
    const char* actLabels[3] = {"Bitir", "Geri al", "Temizle"};
    const Action actActions[3] = {Action::Finish, Action::Undo, Action::Clear};
    for (int i = 0; i < 3; ++i) {
        RectI r;
        r.left = innerL + i * (actW + kGap);
        r.right = (i == 2) ? innerR : r.left + actW;
        r.top = y;
        r.bottom = y - kActionH;
        out[n++] = {actLabels[i], actActions[i], r};
    }
    return n;
}

// Section basliklarinin y konumlari (yazi tabani)
int ModeSectionY(int t) { return t - kTitleH - kGap - kSectionH + 5; }
int ActionSectionY(int t) { return ModeSectionY(t) - 2 * kModeH - kGap - kGap - kSectionH; }
int StatusTop(int t) { return ActionSectionY(t) - 5 - kActionH - kGap * 2; }

bool Contains(const RectI& rc, int x, int y) {
    return x >= rc.left && x <= rc.right && y >= rc.bottom && y <= rc.top;
}

bool IsActive(Action a) {
    Drawings::Mode m = Drawings::GetMode();
    return (a == Action::Pan && m == Drawings::Mode::Pan) ||
           (a == Action::Point && m == Drawings::Mode::Point) ||
           (a == Action::Route && m == Drawings::Mode::Route) ||
           (a == Action::Region && m == Drawings::Mode::Region);
}

bool IsEnabled(Action a) {
    bool hasCurrent = Drawings::Current() != nullptr;
    bool hasAny = hasCurrent || Drawings::ShapeCount() > 0;
    switch (a) {
        case Action::Finish: return hasCurrent;
        case Action::Undo:
        case Action::Clear: return hasAny;
        default: return true;
    }
}

void Perform(Action a) {
    switch (a) {
        case Action::Pan: Drawings::SetMode(Drawings::Mode::Pan); break;
        case Action::Point: Drawings::SetMode(Drawings::Mode::Point); break;
        case Action::Route: Drawings::SetMode(Drawings::Mode::Route); break;
        case Action::Region: Drawings::SetMode(Drawings::Mode::Region); break;
        case Action::Finish: Drawings::Finish(); break;
        case Action::Undo: Drawings::Undo(); break;
        case Action::Clear: Drawings::Clear(); break;
    }
    Overlay::Update();  // pencereleri hemen yeni moda gore ayarla
}

// --- Cizim yardimcilari ------------------------------------------------------------

void Fill(const RectI& r, float cr, float cg, float cb, float ca) {
    glColor4f(cr, cg, cb, ca);
    glBegin(GL_QUADS);
    glVertex2i(r.left, r.bottom);
    glVertex2i(r.right, r.bottom);
    glVertex2i(r.right, r.top);
    glVertex2i(r.left, r.top);
    glEnd();
}

void Frame(const RectI& r, float cr, float cg, float cb, float ca) {
    glColor4f(cr, cg, cb, ca);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(r.left + 0.5f, r.bottom + 0.5f);
    glVertex2f(r.right - 0.5f, r.bottom + 0.5f);
    glVertex2f(r.right - 0.5f, r.top - 0.5f);
    glVertex2f(r.left + 0.5f, r.top - 0.5f);
    glEnd();
}

void HLine(int x0, int x1, int y, float a) {
    glColor4f(0.35f, 0.38f, 0.44f, a);
    glBegin(GL_LINES);
    glVertex2f(float(x0), y + 0.5f);
    glVertex2f(float(x1), y + 0.5f);
    glEnd();
}

void Text(const float* color, int x, int y, const char* s) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s", s);
    float c[3] = {color[0], color[1], color[2]};
    XPLMDrawString(c, x, y, buf, nullptr, xplmFont_Proportional);
}

int TextWidth(const char* s) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s", s);
    return static_cast<int>(XPLMMeasureString(xplmFont_Proportional, buf, static_cast<int>(std::strlen(buf))));
}

const float kWhite[3] = {0.96f, 0.97f, 0.98f};
const float kGrey[3] = {0.62f, 0.66f, 0.72f};
const float kDim[3] = {0.40f, 0.43f, 0.48f};
const float kCyanText[3] = {0.45f, 0.85f, 1.0f};
const float kYellow[3] = {1.0f, 0.80f, 0.25f};

const char* Hint() {
    switch (Drawings::GetMode()) {
        case Drawings::Mode::Pan: return "Haritayi surukleyerek gezin";
        case Drawings::Mode::Point: return "Sol tik: nokta ekle";
        case Drawings::Mode::Route:
        case Drawings::Mode::Region: return "Sol tik: kose  Sag tik: bitir";
    }
    return "";
}

void OnDraw(XPLMWindowID id, void*) {
    Screen::CaptureWindowTransform(Overlay::Diagnostics());

    int l, t, r, b;
    XPLMGetWindowGeometry(id, &l, &t, &r, &b);
    int mx = 0, my = 0;
    XPLMGetMouseLocationGlobal(&mx, &my);

    CullFaceOff cullGuard;  // dolu sekiller gorunsun
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);

    int fontH = 10;
    XPLMGetFontDimensions(xplmFont_Proportional, nullptr, &fontH, nullptr);

    // Zemin, baslik bandi, cerceve
    RectI bg = {l, t, r, b};
    Fill(bg, 0.07f, 0.08f, 0.10f, 0.94f);
    RectI title = {l, t, r, t - kTitleH};
    Fill(title, 0.11f, 0.13f, 0.17f, 1.0f);
    Frame(bg, 0.30f, 0.33f, 0.38f, 1.0f);
    HLine(l, r, t - kTitleH, 1.0f);
    Text(kWhite, l + kPad, t - kTitleH / 2 - fontH / 2 + 1, "HARITA PLUGIN");
    // Isaretleme acikken basliktaki nokta yanar
    if (Drawings::GetMode() != Drawings::Mode::Pan) {
        RectI dot = {r - kPad - 8, t - kTitleH / 2 + 4, r - kPad, t - kTitleH / 2 - 4};
        Fill(dot, 0.0f, 0.75f, 1.0f, 1.0f);
    }

    Text(kGrey, l + kPad, ModeSectionY(t), "MOD");
    Text(kGrey, l + kPad, ActionSectionY(t), "ISLEM");

    // Butonlar
    Button buttons[8];
    int count = Layout(l, t, buttons);
    for (int i = 0; i < count; ++i) {
        const Button& btn = buttons[i];
        bool active = IsActive(btn.action);
        bool enabled = IsEnabled(btn.action);
        bool hover = enabled && Contains(btn.rect, mx, my);

        if (active) {
            Fill(btn.rect, 0.0f, 0.50f, 0.72f, 1.0f);
            Frame(btn.rect, 0.45f, 0.85f, 1.0f, 1.0f);
        } else if (!enabled) {
            Fill(btn.rect, 0.12f, 0.13f, 0.15f, 1.0f);
            Frame(btn.rect, 0.20f, 0.22f, 0.25f, 1.0f);
        } else {
            if (hover) {
                Fill(btn.rect, 0.26f, 0.29f, 0.35f, 1.0f);
            } else {
                Fill(btn.rect, 0.17f, 0.19f, 0.23f, 1.0f);
            }
            Frame(btn.rect, 0.33f, 0.36f, 0.42f, 1.0f);
        }

        int tw = TextWidth(btn.label);
        int tx = (btn.rect.left + btn.rect.right) / 2 - tw / 2;
        int ty = (btn.rect.top + btn.rect.bottom) / 2 - fontH / 2 + 1;
        Text(enabled ? kWhite : kDim, tx, ty, btn.label);
    }

    // Durum
    int y = StatusTop(t);
    HLine(l + kPad, r - kPad, y + 6, 0.8f);
    y -= kLineH;
    const int valueX = l + kPad + 58;
    char value[64];

    Text(kGrey, l + kPad, y, "Mod");
    Text(kWhite, valueX, y, Drawings::ModeName(Drawings::GetMode()));
    y -= kLineH;

    LatLon hover;
    bool hasHover = MapLayer::GetHover(&hover);
    Text(kGrey, l + kPad, y, "Enlem");
    if (hasHover) {
        std::snprintf(value, sizeof(value), "%.5f %c", std::fabs(hover.lat), hover.lat >= 0 ? 'N' : 'S');
        Text(kWhite, valueX, y, value);
    } else {
        Text(kDim, valueX, y, "-");
    }
    y -= kLineH;
    Text(kGrey, l + kPad, y, "Boylam");
    if (hasHover) {
        std::snprintf(value, sizeof(value), "%.5f %c", std::fabs(hover.lon), hover.lon >= 0 ? 'E' : 'W');
        Text(kWhite, valueX, y, value);
    } else {
        Text(kDim, valueX, y, "-");
    }
    y -= kLineH;

    Text(kGrey, l + kPad, y, "Cizim");
    std::snprintf(value, sizeof(value), "%d", Drawings::ShapeCount());
    Text(kWhite, valueX, y, value);
    y -= kLineH;

    Text(kCyanText, l + kPad, y, Hint());
    y -= kLineH;

    const char* warn = Overlay::StatusText();
    if (warn && warn[0]) Text(kYellow, l + kPad, y, warn);
}

int OnClick(XPLMWindowID id, int x, int y, XPLMMouseStatus status, void*) {
    if (status != xplm_MouseDown) return 1;
    int l, t, r, b;
    XPLMGetWindowGeometry(id, &l, &t, &r, &b);
    Button buttons[8];
    int count = Layout(l, t, buttons);
    for (int i = 0; i < count; ++i) {
        if (Contains(buttons[i].rect, x, y) && IsEnabled(buttons[i].action)) {
            Perform(buttons[i].action);
            break;
        }
    }
    return 1;  // panelin ustundeki tiklamayi her zaman tuket
}

int OnRightClick(XPLMWindowID, int, int, XPLMMouseStatus, void*) { return 1; }
int OnWheel(XPLMWindowID, int, int, int, int, void*) { return 1; }
XPLMCursorStatus OnCursor(XPLMWindowID, int, int, void*) { return xplm_CursorArrow; }
void OnKey(XPLMWindowID, char, XPLMKeyFlags, char, void*, int) {}

}  // namespace

void Create() {
    if (g_window) return;
    XPLMCreateWindow_t p;
    std::memset(&p, 0, sizeof(p));
    p.structSize = sizeof(p);
    p.left = 50;
    p.top = 50 + PanelHeight();
    p.right = 50 + kWidth;
    p.bottom = 50;
    p.visible = 0;
    p.drawWindowFunc = OnDraw;
    p.handleMouseClickFunc = OnClick;
    p.handleRightClickFunc = OnRightClick;
    p.handleKeyFunc = OnKey;
    p.handleCursorFunc = OnCursor;
    p.handleMouseWheelFunc = OnWheel;
    p.refcon = nullptr;
    p.decorateAsFloatingWindow = xplm_WindowDecorationNone;  // kendimiz ciziyoruz
    p.layer = xplm_WindowLayerFloatingWindows;
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

int Width() { return kWidth; }
int Height() { return PanelHeight(); }
XPLMWindowID Id() { return g_window; }

}  // namespace ButtonPanel
