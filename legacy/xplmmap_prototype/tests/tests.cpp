// tests.cpp
// X-Plane OLMADAN calisan testler: koordinat matematigi, cografi hesaplar,
// cizim modeli. X-Plane'in harita ve pencere cizerken kurdugu OpenGL
// matrisleri farkli senaryolarla taklit edilir (UI olcek, viewport, donme).
//
// Derleme (ornek): g++ -std=c++17 -Isrc tests/tests.cpp src/Geo.cpp
//                  src/ScreenMath.cpp src/Drawings.cpp -o tests
#include <cmath>
#include <cstdio>
#include <vector>

#include "../src/Drawings.h"
#include "../src/Geo.h"
#include "../src/ScreenMath.h"

// Log.cpp yerine: XPLMDebugString taklidi
extern "C" void XPLMDebugString(const char*) {}
#include <cstdarg>
void Log(const char*, ...) {}

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...)                                         \
    do {                                                         \
        if (cond) {                                              \
            ++g_pass;                                            \
        } else {                                                 \
            ++g_fail;                                            \
            std::printf("HATA %s:%d: ", __FILE__, __LINE__);     \
            std::printf(__VA_ARGS__);                            \
            std::printf("\n");                                   \
        }                                                        \
    } while (0)

static bool Near(double a, double b, double eps) { return std::fabs(a - b) <= eps; }

// ---- OpenGL matris yardimcilari (sutun oncelikli) ----
struct Mat4 { float m[16]; };
static Mat4 Identity() { Mat4 r{}; r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1; return r; }
static Mat4 Mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}
static Mat4 Ortho(double l, double r, double b, double t) {
    Mat4 o = Identity();
    o.m[0] = float(2 / (r - l));
    o.m[5] = float(2 / (t - b));
    o.m[10] = -1;
    o.m[12] = float(-(r + l) / (r - l));
    o.m[13] = float(-(t + b) / (t - b));
    return o;
}
static Mat4 Translate(double x, double y) { Mat4 t = Identity(); t.m[12] = float(x); t.m[13] = float(y); return t; }
static Mat4 Scale(double s) { Mat4 t = Identity(); t.m[0] = t.m[5] = float(s); return t; }
static Mat4 Rotate(double deg) {
    double a = deg * 3.14159265358979 / 180;
    Mat4 t = Identity();
    t.m[0] = float(std::cos(a)); t.m[1] = float(std::sin(a));
    t.m[4] = float(-std::sin(a)); t.m[5] = float(std::cos(a));
    return t;
}
// Gercek OpenGL hattinin yaptigi: nokta -> clip -> NDC -> piksel
static void GLToPixel(const Mat4& mv, const Mat4& pr, const int vp[4], double x, double y, double* px, double* py) {
    Mat4 m = Mul(pr, mv);
    double cx = m.m[0] * x + m.m[4] * y + m.m[12];
    double cy = m.m[1] * x + m.m[5] * y + m.m[13];
    double cw = m.m[3] * x + m.m[7] * y + m.m[15];
    *px = vp[0] + (cx / cw + 1) * 0.5 * vp[2];
    *py = vp[1] + (cy / cw + 1) * 0.5 * vp[3];
}

// ---- Senaryo: X-Plane penceresi + harita ----
struct Scenario {
    const char* name;
    double uiScale;         // UI olcek (piksel / boxel)
    int winW, winH;         // ana pencere, piksel
    Mat4 mapMV, mapPR;      // harita cizilirken matrisler
    int mapVP[4];           // harita cizilirken viewport
    RectD bounds;           // haritanin gorunen alani (harita birimi)
    RectD expectPixels;     // haritanin ekrandaki gercek dikdortgeni
};

static void RunScenario(const Scenario& s) {
    // Pencere cizimi: boxel koordinatlari, tum pencere viewport
    int winVP[4] = {0, 0, s.winW, s.winH};
    Mat4 winMV = Identity();
    Mat4 winPR = Ortho(0, s.winW / s.uiScale, 0, s.winH / s.uiScale);

    Affine2D boxelToPixel, mapToPixel, pixelToBoxel, pixelToMap;
    CHECK(AffineFromGL(winMV.m, winPR.m, winVP, &boxelToPixel), "%s: pencere matrisi", s.name);
    CHECK(AffineFromGL(s.mapMV.m, s.mapPR.m, s.mapVP, &mapToPixel), "%s: harita matrisi", s.name);
    CHECK(Invert(boxelToPixel, &pixelToBoxel) && Invert(mapToPixel, &pixelToMap), "%s: ters", s.name);

    // 1) Afin donusum gercek OpenGL hattiyla ayni sonucu veriyor mu?
    for (double u = s.bounds.left; u <= s.bounds.right; u += s.bounds.Width() / 4) {
        for (double v = s.bounds.bottom; v <= s.bounds.top; v += s.bounds.Height() / 4) {
            double gx, gy, ax, ay;
            GLToPixel(s.mapMV, s.mapPR, s.mapVP, u, v, &gx, &gy);
            mapToPixel.Apply(u, v, &ax, &ay);
            CHECK(Near(gx, ax, 0.05) && Near(gy, ay, 0.05), "%s: harita->piksel (%.1f,%.1f) vs (%.1f,%.1f)",
                  s.name, gx, gy, ax, ay);
        }
    }

    // 2) Haritanin ekrandaki dikdortgeni (donuk degilse) dogru mu?
    RectD px = TransformRectBBox(mapToPixel, s.bounds);
    if (!HasRotation(mapToPixel)) {
        CHECK(Near(px.left, s.expectPixels.left, 0.5) && Near(px.right, s.expectPixels.right, 0.5) &&
                  Near(px.bottom, s.expectPixels.bottom, 0.5) && Near(px.top, s.expectPixels.top, 0.5),
              "%s: piksel dikdortgen (%.1f,%.1f,%.1f,%.1f)", s.name, px.left, px.top, px.right, px.bottom);
    }

    // 3) Boxel dikdortgeni: piksel / uiScale olmali
    RectD bx = TransformRectBBox(pixelToBoxel, s.expectPixels);
    CHECK(Near(bx.left, s.expectPixels.left / s.uiScale, 0.01) && Near(bx.top, s.expectPixels.top / s.uiScale, 0.01),
          "%s: boxel dikdortgen", s.name);

    // 4) Tiklama zinciri: harita birimindeki bir nokta ekranda nereye
    //    dusuyorsa oraya (boxel) tiklayinca ayni harita birimine donmeli
    for (int i = 0; i < 25; ++i) {
        double u = s.bounds.left + s.bounds.Width() * ((i % 5) + 0.5) / 5;
        double v = s.bounds.bottom + s.bounds.Height() * ((i / 5) + 0.5) / 5;
        double gx, gy;
        GLToPixel(s.mapMV, s.mapPR, s.mapVP, u, v, &gx, &gy);   // ekranda nerede
        int clickX = int(std::lround(gx / s.uiScale));          // fare boxel olarak (tamsayi)
        int clickY = int(std::lround(gy / s.uiScale));

        double ppx, ppy, mu, mv;
        boxelToPixel.Apply(clickX, clickY, &ppx, &ppy);  // Overlay::HandleMapClick ile ayni adimlar
        pixelToMap.Apply(ppx, ppy, &mu, &mv);

        // Boxel tamsayi oldugu icin en fazla yarim boxel hata beklenir
        double unitsPerBoxel = s.uiScale / std::sqrt(std::fabs(mapToPixel.a * mapToPixel.d - mapToPixel.b * mapToPixel.c));
        double err = std::hypot(mu - u, mv - v) / unitsPerBoxel;
        CHECK(err <= 0.75, "%s: tiklama hatasi %.3f boxel", s.name, err);
    }
}

static void TestScreenMath() {
    // A) Viewport tum pencere, harita matrisi piksele olcekliyor, UI %150
    {
        Scenario s{};
        s.name = "A: tam viewport, %150";
        s.uiScale = 1.5;
        s.winW = 2560; s.winH = 1440;
        s.bounds = {-50000, 30000, 70000, -50000};  // sol, ust, sag, alt (harita birimi)
        s.expectPixels = {600, 1100, 1800, 300};
        double k = 1200.0 / 120000.0;  // piksel / harita birimi
        s.mapPR = Ortho(0, 2560, 0, 1440);
        s.mapMV = Mul(Translate(600, 300), Mul(Scale(k), Translate(50000, 50000)));
        int vp[4] = {0, 0, 2560, 1440};
        for (int i = 0; i < 4; ++i) s.mapVP[i] = vp[i];
        RunScenario(s);
    }
    // B) Viewport sadece harita alani, projeksiyon dogrudan harita birimi, UI %100
    {
        Scenario s{};
        s.name = "B: harita viewport, %100";
        s.uiScale = 1.0;
        s.winW = 1920; s.winH = 1080;
        s.bounds = {1000, 9000, 9000, 3000};
        s.expectPixels = {400, 900, 1200, 300};
        s.mapPR = Ortho(1000, 9000, 3000, 9000);
        s.mapMV = Identity();
        int vp[4] = {400, 300, 800, 600};
        for (int i = 0; i < 4; ++i) s.mapVP[i] = vp[i];
        RunScenario(s);
    }
    // C) Donuk harita (track-up 30 derece), UI %125
    {
        Scenario s{};
        s.name = "C: donuk harita 30 derece, %125";
        s.uiScale = 1.25;
        s.winW = 1920; s.winH = 1080;
        s.bounds = {-1000, 1000, 1000, -1000};
        s.expectPixels = {660, 840, 1260, 240};  // sadece 3. adimda kullanilir
        s.mapPR = Ortho(0, 1920, 0, 1080);
        s.mapMV = Mul(Translate(960, 540), Mul(Rotate(30), Scale(0.3)));
        int vp[4] = {0, 0, 1920, 1080};
        for (int i = 0; i < 4; ++i) s.mapVP[i] = vp[i];
        RunScenario(s);
        Affine2D m;
        AffineFromGL(s.mapMV.m, s.mapPR.m, s.mapVP, &m);
        CHECK(HasRotation(m), "C: donme algilanmali");
    }

    // Perspektif matris reddedilmeli
    {
        Mat4 pr = Identity();
        pr.m[11] = -1; pr.m[15] = 0;  // perspektif
        Mat4 mv = Identity();
        int vp[4] = {0, 0, 100, 100};
        Affine2D a;
        CHECK(!AffineFromGL(mv.m, pr.m, vp, &a), "perspektif reddedilmeli");
    }

    // Compose / Invert
    {
        Affine2D a; a.a = 2; a.b = 0.5; a.c = -0.3; a.d = 1.5; a.tx = 10; a.ty = -4;
        Affine2D inv, id;
        CHECK(Invert(a, &inv), "ters alinmali");
        id = Compose(inv, a);
        CHECK(Near(id.a, 1, 1e-12) && Near(id.d, 1, 1e-12) && Near(id.b, 0, 1e-12) && Near(id.c, 0, 1e-12) &&
                  Near(id.tx, 0, 1e-9) && Near(id.ty, 0, 1e-9),
              "A^-1 * A = birim");
    }

    // Kesisim
    {
        RectD a{0, 100, 100, 0}, b{50, 150, 150, 50}, o;
        CHECK(IntersectRect(a, b, &o) && o.left == 50 && o.top == 100 && o.right == 100 && o.bottom == 50, "kesisim");
        RectD c{200, 300, 300, 200};
        CHECK(!IntersectRect(a, c, &o), "kesismeyen");
    }
}

static double TriArea(const std::vector<LatLon>& p, const std::vector<int>& t) {
    double s = 0;
    for (size_t i = 0; i + 2 < t.size(); i += 3) {
        const LatLon &a = p[t[i]], &b = p[t[i + 1]], &c = p[t[i + 2]];
        s += std::fabs((b.lon - a.lon) * (c.lat - a.lat) - (c.lon - a.lon) * (b.lat - a.lat)) / 2;
    }
    return s;
}
static double PolyArea(const std::vector<LatLon>& p) {
    double s = 0;
    for (size_t i = 0; i < p.size(); ++i) {
        const LatLon &a = p[i], &b = p[(i + 1) % p.size()];
        s += a.lon * b.lat - b.lon * a.lat;
    }
    return std::fabs(s) / 2;
}

static void TestGeo() {
    LatLon ltfm{41.2620, 28.7420}, ltac{40.1281, 32.9951};
    double d = DistanceNm(ltfm, ltac);
    CHECK(d > 200 && d < 210, "LTFM-LTAC mesafe %.1f NM (gercek ~380 km = ~205 NM)", d);
    CHECK(Near(DistanceNm({0, 0}, {0, 1}), 60.04, 0.1), "ekvatorda 1 derece ~60 NM");

    LatLon mid = GreatCircleInterpolate(ltfm, ltac, 0.5);
    CHECK(Near(DistanceNm(ltfm, mid), d / 2, 0.01), "orta nokta");

    std::vector<LatLon> dense;
    DensifySegment(ltfm, ltac, 5.0, dense);
    CHECK(dense.size() >= 41 && dense.size() <= 42, "5 NM parcalara bolme: %d", (int)dense.size());
    CHECK(Near(dense[0].lat, ltfm.lat, 1e-12), "ilk nokta a");
    std::vector<LatLon> shortSeg;
    DensifySegment(ltfm, {41.27, 28.75}, 5.0, shortSeg);
    CHECK(shortSeg.size() == 1, "kisa segment bolunmez");

    // Kare
    std::vector<LatLon> sq = {{41, 29}, {41, 30}, {42, 30}, {42, 29}};
    std::vector<int> t;
    CHECK(TriangulatePolygon(sq, t) && t.size() == 6, "kare 2 ucgen");
    CHECK(Near(TriArea(sq, t), PolyArea(sq), 1e-9), "kare alan");

    // Saat yonunde L sekli (icbukey)
    std::vector<LatLon> L = {{0, 0}, {2, 0}, {2, 1}, {1, 1}, {1, 2}, {0, 2}};
    CHECK(TriangulatePolygon(L, t) && t.size() == 12, "L 4 ucgen");
    CHECK(Near(TriArea(L, t), PolyArea(L), 1e-9), "L alan %.3f vs %.3f", TriArea(L, t), PolyArea(L));
    std::vector<LatLon> Lrev(L.rbegin(), L.rend());
    CHECK(TriangulatePolygon(Lrev, t) && Near(TriArea(Lrev, t), PolyArea(Lrev), 1e-9), "ters yon L");

    // Kendini kesen (papyon)
    std::vector<LatLon> bow = {{0, 0}, {1, 1}, {1, 0}, {0, 1}};
    CHECK(!TriangulatePolygon(bow, t), "papyon reddedilmeli");

    // Ayni dogru uzerinde kose
    std::vector<LatLon> col = {{0, 0}, {0, 1}, {0, 2}, {1, 2}, {1, 0}};
    CHECK(TriangulatePolygon(col, t) && Near(TriArea(col, t), PolyArea(col), 1e-9), "dogrusal kose");

    // Dogru (alansiz)
    std::vector<LatLon> line = {{0, 0}, {0, 1}, {0, 2}};
    CHECK(!TriangulatePolygon(line, t), "alansiz reddedilmeli");
}

static void TestDrawings() {
    using namespace Drawings;
    Clear();
    SetMode(Mode::Pan);
    AddVertex({1, 1});
    CHECK(ShapeCount() == 0, "gezin modunda ekleme yok");

    SetMode(Mode::Point);
    AddVertex({1, 1});
    AddVertex({2, 2});
    CHECK(ShapeCount() == 2 && Shapes()[1].number == 2, "iki nokta P1 P2");

    SetMode(Mode::Route);
    AddVertex({0, 0});
    CHECK(Current() && Current()->pts.size() == 1, "rota basladi");
    SetMode(Mode::Region);  // 1 koseli rota atilmali
    CHECK(ShapeCount() == 2 && !Current(), "1 koseli rota atildi");

    AddVertex({0, 0});
    AddVertex({0, 1});
    AddVertex({1, 1});
    Finish();
    CHECK(ShapeCount() == 3 && Shapes()[2].kind == ShapeKind::Region && Shapes()[2].triangles.size() == 3,
          "bolge B1 ucgenlendi");

    SetMode(Mode::Route);
    AddVertex({0, 0});
    AddVertex({0, 1});
    AddVertex({0, 2});
    Undo();
    CHECK(Current() && Current()->pts.size() == 2, "geri al: kose");
    Finish();
    CHECK(ShapeCount() == 4 && Shapes()[3].number == 1 && Shapes()[3].pts.size() == 2, "rota R1");
    Undo();
    CHECK(ShapeCount() == 3, "geri al: sekil");
    AddVertex({5, 5});
    AddVertex({5, 6});
    Finish();
    CHECK(Shapes()[3].number == 1, "numara geri alinca tekrar kullanilir");

    Clear();
    CHECK(ShapeCount() == 0 && !Current(), "temizle");
    SetMode(Mode::Pan);
}

int main() {
    TestScreenMath();
    TestGeo();
    TestDrawings();
    std::printf("%d test gecti, %d hata\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
