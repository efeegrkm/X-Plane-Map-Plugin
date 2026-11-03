// MapLayer.cpp
#include "MapLayer.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMMap.h"
#include "XPLMProcessing.h"

#include "Drawings.h"
#include "GLInclude.h"
#include "Log.h"
#include "Overlay.h"
#include "ScreenState.h"

namespace MapLayer {

namespace {

// --- Durum -------------------------------------------------------------------

XPLMMapLayerID g_layer = nullptr;
std::string g_iconPath;   // ikon PNG'si (X-Plane klasorune gore)
bool g_iconOk = false;
bool g_diag = false;

ScreenInfo g_info;

std::vector<std::pair<double, double>> g_pendingClicks;  // harita biriminde

bool g_hoverValid = false;
LatLon g_hover = {0, 0};
float g_hoverMapX = 0, g_hoverMapY = 0;

bool g_demoSet = false;
LatLon g_demoFrom = {0, 0}, g_demoTo = {0, 0};
char g_demoLabel[96] = "";

std::vector<LatLon> g_trail;

const double kDensifyNm = 5.0;  // uzun cizgileri 5 NM'lik parcalara bol (egri dogru cizilsin)

// --- Kucuk yardimcilar ---------------------------------------------------------

bool InBounds(float x, float y, const float* ltrb, float margin) {
    float minX = std::fmin(ltrb[0], ltrb[2]) - margin, maxX = std::fmax(ltrb[0], ltrb[2]) + margin;
    float minY = std::fmin(ltrb[1], ltrb[3]) - margin, maxY = std::fmax(ltrb[1], ltrb[3]) + margin;
    return x >= minX && x <= maxX && y >= minY && y <= maxY;
}

void Project(XPLMMapProjectionID proj, const LatLon& p, float* x, float* y) {
    XPLMMapProject(proj, p.lat, p.lon, x, y);
}

struct Color {
    float r, g, b, a;
};
const Color kCyan = {0.0f, 0.72f, 0.95f, 1.0f};     // nokta ve rota
const Color kGreen = {0.10f, 0.75f, 0.35f, 1.0f};   // bolge
const Color kOrange = {1.0f, 0.50f, 0.05f, 1.0f};   // yarim kalan cizim
const Color kMagenta = {0.85f, 0.15f, 0.85f, 0.9f}; // demo rota
const Color kYellow = {0.95f, 0.75f, 0.0f, 0.9f};   // ucak izi
const Color kWhite = {1.0f, 1.0f, 1.0f, 1.0f};
const Color kShadow = {0.0f, 0.0f, 0.0f, 0.45f};    // cizgilerin altindaki koyu kenar

void SetColor(const Color& c, float alphaScale = 1.0f) { glColor4f(c.r, c.g, c.b, c.a * alphaScale); }

// Lat/lon listesini, uzun segmentleri buyuk daire boyunca bolerek harita
// koordinatina cevirir (cizgi haritanin projeksiyonuna uygun egilsin).
std::vector<std::pair<float, float>> ProjectPath(XPLMMapProjectionID proj, const std::vector<LatLon>& pts,
                                                 bool closed) {
    std::vector<LatLon> dense;
    for (size_t i = 0; i + 1 < pts.size(); ++i) DensifySegment(pts[i], pts[i + 1], kDensifyNm, dense);
    if (closed && pts.size() >= 3) {
        DensifySegment(pts.back(), pts.front(), kDensifyNm, dense);
    } else if (!pts.empty()) {
        dense.push_back(pts.back());
    }
    std::vector<std::pair<float, float>> out;
    out.reserve(dense.size());
    for (const auto& p : dense) {
        float x, y;
        Project(proj, p, &x, &y);
        out.emplace_back(x, y);
    }
    return out;
}

void LineStrip(const std::vector<std::pair<float, float>>& v, bool closed) {
    glBegin(closed ? GL_LINE_LOOP : GL_LINE_STRIP);
    for (const auto& p : v) glVertex2f(p.first, p.second);
    glEnd();
}

// Koyu bir kenarla cizgi: her zemin rengi uzerinde okunur kalsin
void DrawPath(XPLMMapProjectionID proj, const std::vector<LatLon>& pts, bool closed, float width,
              const Color& c) {
    if (pts.size() < 2) return;
    auto v = ProjectPath(proj, pts, closed);
    glLineWidth(width + 2.0f);
    SetColor(kShadow);
    LineStrip(v, closed);
    glLineWidth(width);
    SetColor(c);
    LineStrip(v, closed);
}

// Dolu daire (harita biriminde merkez, yaricap)
void Disc(float x, float y, float radius) {
    const int kSeg = 20;
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    for (int i = 0; i <= kSeg; ++i) {
        float a = 6.2831853f * i / kSeg;
        glVertex2f(x + radius * std::cos(a), y + radius * std::sin(a));
    }
    glEnd();
}

// Isaret: beyaz kenarli renkli daire. size: UI birimi (ekranda sabit boyut)
void Marker(float x, float y, float upu, float size, const Color& c, bool centerDot) {
    SetColor(kShadow);
    Disc(x, y, (size + 1.5f) * upu);
    SetColor(kWhite);
    Disc(x, y, size * upu);
    SetColor(c);
    Disc(x, y, (size - 2.0f) * upu);
    if (centerDot) {
        glColor4f(0.05f, 0.12f, 0.2f, 1.0f);
        Disc(x, y, 1.8f * upu);
    }
}

void Markers(XPLMMapProjectionID proj, const std::vector<LatLon>& pts, float upu, float size, const Color& c) {
    for (const auto& p : pts) {
        float x, y;
        Project(proj, p, &x, &y);
        Marker(x, y, upu, size, c, false);
    }
}

double PathLengthNm(const std::vector<LatLon>& pts) {
    double d = 0;
    for (size_t i = 0; i + 1 < pts.size(); ++i) d += DistanceNm(pts[i], pts[i + 1]);
    return d;
}

void Label(XPLMMapLayerID layer, const char* text, float x, float y) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s", text);  // SDK surumleri arasi uyum icin degistirilebilir kopya
    XPLMDrawMapLabel(layer, buf, x, y, xplm_MapOrientation_UI, 0.0f);
}

// --- Draw callback'in parcalari ----------------------------------------------------

// 1) OpenGL matrisini ve haritanin ekrandaki yerini kaydet
void CaptureScreenInfo(const float* ltrb, float now) {
    float mv[16], pr[16];
    GLint vp[4];
    glGetFloatv(GL_MODELVIEW_MATRIX, mv);
    glGetFloatv(GL_PROJECTION_MATRIX, pr);
    glGetIntegerv(GL_VIEWPORT, vp);

    ScreenInfo info;
    info.drawTime = now;
    for (int i = 0; i < 4; ++i) info.viewport[i] = vp[i];
    info.boundsMap.left = ltrb[0];
    info.boundsMap.top = ltrb[1];
    info.boundsMap.right = ltrb[2];
    info.boundsMap.bottom = ltrb[3];

    info.valid = AffineFromGL(mv, pr, info.viewport, &info.mapToPixel);
    if (info.valid) {
        info.rotated = HasRotation(info.mapToPixel);

        // Gorunen alanin koseleri -> piksel
        RectD rect = TransformRectBBox(info.mapToPixel, info.boundsMap);

        // Viewport disina tasamaz
        RectD vpRect;
        vpRect.left = vp[0];
        vpRect.bottom = vp[1];
        vpRect.right = vp[0] + vp[2];
        vpRect.top = vp[1] + vp[3];
        RectD clipped;
        if (IntersectRect(rect, vpRect, &clipped)) rect = clipped;

        // X-Plane haritayi bir scissor (kirpma) kutusuyla ciziyorsa o kutu
        // haritanin tam alanidir; harita donukken bile dogru sonuc verir.
        if (glIsEnabled(GL_SCISSOR_TEST)) {
            GLint box[4];
            glGetIntegerv(GL_SCISSOR_BOX, box);
            RectD sc;
            sc.left = box[0];
            sc.bottom = box[1];
            sc.right = box[0] + box[2];
            sc.top = box[1] + box[3];
            RectD both;
            if (IntersectRect(rect, sc, &both)) {
                info.rectFromScissor = std::fabs(both.Width() - rect.Width()) > 0.5 ||
                                       std::fabs(both.Height() - rect.Height()) > 0.5;
                rect = both;
            }
        }
        info.mapRectPixels = rect;

        if (g_diag) {
            GLint fb = -1;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fb);
            while (glGetError() != GL_NO_ERROR) {}
            info.framebuffer = fb;
        }
    } else {
        static bool warned = false;
        if (!warned) {
            Log("UYARI: harita matrisi 2B degil, tiklama hesabi yapilamaz");
            warned = true;
        }
    }
    g_info = info;
}

// 2) Kuyruktaki tiklamalari lat/lon'a cevirip cizimlere ekle
void ResolvePendingClicks(XPLMMapProjectionID proj) {
    for (const auto& c : g_pendingClicks) {
        double lat = 0, lon = 0;
        XPLMMapUnproject(proj, static_cast<float>(c.first), static_cast<float>(c.second), &lat, &lon);
        LatLon p = {lat, lon};
        Drawings::AddVertex(p);
    }
    g_pendingClicks.clear();
}

// 3) Imlecin altindaki lat/lon'u hesapla
void UpdateHover(XPLMMapProjectionID proj) {
    g_hoverValid = false;
    if (!g_info.valid || !Screen::HasWindowTransform()) return;

    int mx = 0, my = 0;
    XPLMGetMouseLocationGlobal(&mx, &my);  // boxel

    double px, py;
    if (!Screen::BoxelToPixel(mx, my, &px, &py)) return;  // boxel -> piksel

    const RectD& r = g_info.mapRectPixels;
    if (px < r.left || px > r.right || py < r.bottom || py > r.top) return;  // harita disinda

    Affine2D pixelToMap;
    if (!Invert(g_info.mapToPixel, &pixelToMap)) return;
    double ux, uy;
    pixelToMap.Apply(px, py, &ux, &uy);  // piksel -> harita birimi

    double lat = 0, lon = 0;
    XPLMMapUnproject(proj, static_cast<float>(ux), static_cast<float>(uy), &lat, &lon);  // -> lat/lon
    g_hover.lat = lat;
    g_hover.lon = lon;
    g_hoverMapX = static_cast<float>(ux);
    g_hoverMapY = static_cast<float>(uy);
    g_hoverValid = true;
}

bool ShowHover() { return g_hoverValid && (Drawings::GetMode() != Drawings::Mode::Pan || g_diag); }

// --- X-Plane callback'leri -----------------------------------------------------------

void OnWillBeDeleted(XPLMMapLayerID layer, void*) {
    if (layer == g_layer) {
        g_layer = nullptr;
        Log("harita katmani silindi (harita kapatildi/yeniden yuklenecek)");
    }
}

void OnPrepCache(XPLMMapLayerID, const float*, XPLMMapProjectionID, void*) {
    // Projeksiyon sadece burada degisebilir. Harita birimindeki bekleyen
    // tiklamalar artik gecersiz olabilir; guvenli tarafta kalip atiyoruz.
    if (!g_pendingClicks.empty()) {
        Log("projeksiyon degisti, %d bekleyen tiklama atildi", static_cast<int>(g_pendingClicks.size()));
        g_pendingClicks.clear();
    }
}

void OnDraw(XPLMMapLayerID, const float* ltrb, float /*zoomRatio*/, float upu,
            XPLMMapStyle, XPLMMapProjectionID proj, void*) {
    const float now = XPLMGetElapsedTime();

    CaptureScreenInfo(ltrb, now);
    ResolvePendingClicks(proj);
    UpdateHover(proj);
    Overlay::OnMapDrawn();

    // Cizimlerimizi haritanin alaniyla kirp (kenarlardan tasmasin).
    GLboolean scissorWas = glIsEnabled(GL_SCISSOR_TEST);
    GLint scissorBox[4];
    glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
    if (g_info.valid) {
        const RectD& r = g_info.mapRectPixels;
        GLint x = static_cast<GLint>(std::floor(r.left));
        GLint y = static_cast<GLint>(std::floor(r.bottom));
        glEnable(GL_SCISSOR_TEST);
        glScissor(x, y, static_cast<GLsizei>(std::ceil(r.right) - x), static_cast<GLsizei>(std::ceil(r.top) - y));
    }

    // Dolu sekillerin gorunmesi icin yuz ayiklamayi kapat (blok sonunda geri acilir)
    CullFaceOff cullGuard;

    // fog yok, doku yok, isik yok, alpha test yok, saydamlik var, derinlik yok
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);

    // Demo rota (X-Plane nav verisinden bulunan iki havalimani arasi)
    if (g_demoSet) {
        std::vector<LatLon> demo = {g_demoFrom, g_demoTo};
        DrawPath(proj, demo, false, 2.5f, kMagenta);
    }

    // Ucak izi
    if (g_trail.size() >= 2) DrawPath(proj, g_trail, false, 2.0f, kYellow);

    // Bitmis sekiller: once bolgeler (altta), sonra rotalar, en ustte noktalar
    for (const auto& s : Drawings::Shapes()) {
        if (s.kind != Drawings::ShapeKind::Region) continue;
        if (!s.triangles.empty()) {
            SetColor(kGreen, 0.28f);  // yari saydam dolgu
            glBegin(GL_TRIANGLES);
            for (int idx : s.triangles) {
                float x, y;
                Project(proj, s.pts[idx], &x, &y);
                glVertex2f(x, y);
            }
            glEnd();
        }
        DrawPath(proj, s.pts, true, 2.5f, kGreen);
        Markers(proj, s.pts, upu, 3.5f, kGreen);
    }
    for (const auto& s : Drawings::Shapes()) {
        if (s.kind != Drawings::ShapeKind::Route) continue;
        DrawPath(proj, s.pts, false, 3.0f, kCyan);
        Markers(proj, s.pts, upu, 3.5f, kCyan);
    }
    for (const auto& s : Drawings::Shapes()) {
        if (s.kind != Drawings::ShapeKind::Point) continue;
        float x, y;
        Project(proj, s.pts[0], &x, &y);
        Marker(x, y, upu, 7.0f, kCyan, true);
    }

    // Yarim kalan cizim (turuncu) + son koseden imlece kesikli "lastik" cizgi
    if (const Drawings::Shape* cur = Drawings::Current()) {
        bool region = cur->kind == Drawings::ShapeKind::Region;
        if (region && cur->pts.size() >= 3) {
            std::vector<int> tris;
            if (TriangulatePolygon(cur->pts, tris)) {  // olusan alani onizle
                SetColor(kOrange, 0.18f);
                glBegin(GL_TRIANGLES);
                for (int idx : tris) {
                    float x, y;
                    Project(proj, cur->pts[idx], &x, &y);
                    glVertex2f(x, y);
                }
                glEnd();
            }
        }
        DrawPath(proj, cur->pts, false, 2.5f, kOrange);
        if (g_hoverValid && !cur->pts.empty()) {
            float x0, y0;
            Project(proj, cur->pts.back(), &x0, &y0);
            glLineWidth(2.0f);
            SetColor(kOrange);
            glEnable(GL_LINE_STIPPLE);
            glLineStipple(3, 0x5555);
            glBegin(GL_LINES);
            glVertex2f(x0, y0);
            glVertex2f(g_hoverMapX, g_hoverMapY);
            if (region && cur->pts.size() >= 2) {
                float x1, y1;
                Project(proj, cur->pts.front(), &x1, &y1);
                glVertex2f(g_hoverMapX, g_hoverMapY);
                glVertex2f(x1, y1);
            }
            glEnd();
            glDisable(GL_LINE_STIPPLE);
        }
        Markers(proj, cur->pts, upu, 4.0f, kOrange);
    }

    // Imlec artisi: hesaplanan konum imlecle tam ust uste olmali
    if (ShowHover()) {
        const float len = 10.0f * upu, gap = 3.0f * upu;
        const float x = g_hoverMapX, y = g_hoverMapY;
        for (int pass = 0; pass < 2; ++pass) {  // once koyu kenar, sonra beyaz
            glLineWidth(pass == 0 ? 3.5f : 1.5f);
            if (pass == 0) SetColor(kShadow, 1.6f); else SetColor(kWhite);
            glBegin(GL_LINES);
            glVertex2f(x - len, y); glVertex2f(x - gap, y);
            glVertex2f(x + gap, y); glVertex2f(x + len, y);
            glVertex2f(x, y - len); glVertex2f(x, y - gap);
            glVertex2f(x, y + gap); glVertex2f(x, y + len);
            glEnd();
        }
    }

    // Durumu geri yukle
    glLineWidth(1.0f);
    if (scissorWas) {
        glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
}

void OnIcons(XPLMMapLayerID layer, const float* ltrb, float, float upu,
             XPLMMapStyle, XPLMMapProjectionID proj, void*) {
    // Noktalar artik OpenGL isaretiyle ciziliyor. X-Plane'in ikon fonksiyonu
    // (XPLMDrawMapIconFromSheet) sadece teshis modunda, calistigini gormek
    // icin ayrica cizilir: isaretin ustunde biraz sagda kucuk bir ikon.
    if (!g_iconOk || !g_diag) return;
    // ikon sayfasi: 2 hucre yan yana (0: nokta, 1: rota kosesi)
    for (const auto& s : Drawings::Shapes()) {
        if (s.kind != Drawings::ShapeKind::Point) continue;
        float x, y;
        Project(proj, s.pts[0], &x, &y);
        if (!InBounds(x, y, ltrb, 20 * upu)) continue;
        XPLMDrawMapIconFromSheet(layer, g_iconPath.c_str(), 0, 0, 2, 1, x + 16 * upu, y + 16 * upu,
                                 xplm_MapOrientation_UI, 0.0f, 14.0f * upu);
    }
}

void OnLabels(XPLMMapLayerID layer, const float* ltrb, float, float upu,
              XPLMMapStyle, XPLMMapProjectionID proj, void*) {
    char text[96];
    const float margin = 40 * upu;

    for (const auto& s : Drawings::Shapes()) {
        float x, y;
        switch (s.kind) {
            case Drawings::ShapeKind::Point:
                Project(proj, s.pts[0], &x, &y);
                std::snprintf(text, sizeof(text), "P%d", s.number);
                if (InBounds(x, y, ltrb, margin)) Label(layer, text, x, y + 20 * upu);
                break;

            case Drawings::ShapeKind::Route:
                Project(proj, s.pts.back(), &x, &y);
                std::snprintf(text, sizeof(text), "R%d  %.1f NM", s.number, PathLengthNm(s.pts));
                if (InBounds(x, y, ltrb, margin)) Label(layer, text, x, y + 16 * upu);
                break;

            case Drawings::ShapeKind::Region: {
                float cx = 0, cy = 0;
                for (const auto& p : s.pts) {
                    Project(proj, p, &x, &y);
                    cx += x;
                    cy += y;
                }
                cx /= s.pts.size();
                cy /= s.pts.size();
                std::snprintf(text, sizeof(text), "B%d", s.number);
                if (InBounds(cx, cy, ltrb, margin)) Label(layer, text, cx, cy);
                break;
            }
        }
    }

    if (const Drawings::Shape* cur = Drawings::Current()) {
        if (cur->kind == Drawings::ShapeKind::Route && cur->pts.size() >= 2) {
            float x, y;
            Project(proj, cur->pts.back(), &x, &y);
            std::snprintf(text, sizeof(text), "%.1f NM", PathLengthNm(cur->pts));
            Label(layer, text, x, y + 12 * upu);
        }
    }

    if (g_demoSet) {
        LatLon mid = GreatCircleInterpolate(g_demoFrom, g_demoTo, 0.5);
        float x, y;
        Project(proj, mid, &x, &y);
        if (InBounds(x, y, ltrb, margin)) Label(layer, g_demoLabel, x, y + 12 * upu);
    }

    if (ShowHover()) {
        std::snprintf(text, sizeof(text), "%.5f %c  %.5f %c", std::fabs(g_hover.lat),
                      g_hover.lat >= 0 ? 'N' : 'S', std::fabs(g_hover.lon), g_hover.lon >= 0 ? 'E' : 'W');
        Label(layer, text, g_hoverMapX + 70 * upu, g_hoverMapY - 14 * upu);
    }
}

void CreateLayer() {
    if (g_layer) return;
    XPLMCreateMapLayer_t params;
    std::memset(&params, 0, sizeof(params));
    params.structSize = sizeof(params);
    params.mapToCreateLayerIn = XPLM_MAP_USER_INTERFACE;
    params.layerType = xplm_MapLayer_Markings;
    params.willBeDeletedCallback = OnWillBeDeleted;
    params.prepCacheCallback = OnPrepCache;
    params.drawCallback = OnDraw;
    params.iconCallback = OnIcons;
    params.labelCallback = OnLabels;
    params.showUiToggle = 1;  // haritanin katman listesinde ac/kapa kutusu
    params.layerName = "Harita Plugin";
    params.refcon = nullptr;
    g_layer = XPLMCreateMapLayer(&params);
    Log(g_layer ? "harita katmani olusturuldu" : "HATA: harita katmani olusturulamadi");
}

void OnMapCreated(const char* mapId, void*) {
    if (std::strcmp(mapId, XPLM_MAP_USER_INTERFACE) == 0) CreateLayer();
}

}  // namespace

// --- Disariya acik fonksiyonlar -------------------------------------------------------

void Init(const std::string& iconPathRelative, bool iconExists) {
    g_iconPath = iconPathRelative;
    g_iconOk = iconExists;
    XPLMRegisterMapCreationHook(OnMapCreated, nullptr);
    if (XPLMMapExists(XPLM_MAP_USER_INTERFACE)) CreateLayer();
}

void Shutdown() {
    if (g_layer) {
        XPLMDestroyMapLayer(g_layer);
        g_layer = nullptr;
    }
}

const ScreenInfo& Info() { return g_info; }

bool IsMapVisible() {
    if (!g_layer || g_info.drawTime < 0) return false;
    return XPLMGetElapsedTime() - g_info.drawTime < 0.5f;
}

void QueueClick(double mapX, double mapY) { g_pendingClicks.emplace_back(mapX, mapY); }

bool GetHover(LatLon* out) {
    if (!g_hoverValid || !IsMapVisible()) return false;
    *out = g_hover;
    return true;
}

void SetDemoRoute(const LatLon& from, const char* fromName, const LatLon& to, const char* toName) {
    g_demoFrom = from;
    g_demoTo = to;
    g_demoSet = true;
    std::snprintf(g_demoLabel, sizeof(g_demoLabel), "%s - %s  %.0f NM (demo)", fromName, toName,
                  DistanceNm(from, to));
}

void AddTrailPoint(const LatLon& p) {
    if (std::fabs(p.lat) < 1e-6 && std::fabs(p.lon) < 1e-6) return;  // ucak henuz yuklenmedi
    if (!g_trail.empty() && DistanceNm(g_trail.back(), p) < 0.05) return;  // ~90 m'den az hareket
    g_trail.push_back(p);
    if (g_trail.size() > 20000) g_trail.erase(g_trail.begin(), g_trail.begin() + 1000);
}

void ClearTrail() { g_trail.clear(); }

void SetDiagnostics(bool on) { g_diag = on; }

}  // namespace MapLayer
