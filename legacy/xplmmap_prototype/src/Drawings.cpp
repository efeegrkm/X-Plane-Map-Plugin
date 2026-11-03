// Drawings.cpp
#include "Drawings.h"

#include "Log.h"

namespace Drawings {

namespace {
Mode g_mode = Mode::Pan;
std::vector<Shape> g_shapes;
Shape g_current;            // yarim kalan rota/bolge
bool g_hasCurrent = false;
int g_counter[3] = {0, 0, 0};  // tur basina numara sayaci: nokta, rota, bolge

int& CounterFor(ShapeKind k) { return g_counter[static_cast<int>(k)]; }
}  // namespace

Mode GetMode() { return g_mode; }

const char* ModeName(Mode mode) {
    switch (mode) {
        case Mode::Pan: return "Gezin";
        case Mode::Point: return "Nokta";
        case Mode::Route: return "Rota";
        case Mode::Region: return "Bolge";
    }
    return "?";
}

void SetMode(Mode mode) {
    if (mode == g_mode) return;
    Finish();
    g_mode = mode;
    Log("mod: %s", ModeName(mode));
}

void AddVertex(const LatLon& p) {
    switch (g_mode) {
        case Mode::Pan:
            return;  // gezinme modunda isaretleme yok

        case Mode::Point: {
            Shape s;
            s.kind = ShapeKind::Point;
            s.number = ++CounterFor(ShapeKind::Point);
            s.pts.push_back(p);
            g_shapes.push_back(s);
            Log("P%d eklendi: %.5f, %.5f", s.number, p.lat, p.lon);
            return;
        }

        case Mode::Route:
        case Mode::Region: {
            ShapeKind kind = (g_mode == Mode::Route) ? ShapeKind::Route : ShapeKind::Region;
            if (!g_hasCurrent) {
                g_current = Shape();
                g_current.kind = kind;
                g_hasCurrent = true;
            }
            g_current.pts.push_back(p);
            Log("%s kosesi %d: %.5f, %.5f", ModeName(g_mode),
                static_cast<int>(g_current.pts.size()), p.lat, p.lon);
            return;
        }
    }
}

void Finish() {
    if (!g_hasCurrent) return;
    g_hasCurrent = false;

    Shape s = g_current;
    g_current = Shape();

    size_t need = (s.kind == ShapeKind::Route) ? 2 : 3;
    if (s.pts.size() < need) {
        Log("yarim cizim atildi (%d kose, en az %d gerekli)",
            static_cast<int>(s.pts.size()), static_cast<int>(need));
        return;
    }
    if (s.kind == ShapeKind::Region) {
        if (!TriangulatePolygon(s.pts, s.triangles)) {
            Log("bolge kendini kesiyor: sadece kenarlari cizilecek");
        }
    }
    s.number = ++CounterFor(s.kind);
    g_shapes.push_back(s);
    Log("%s%d bitti (%d kose)", s.kind == ShapeKind::Route ? "R" : "B", s.number,
        static_cast<int>(s.pts.size()));
}

void Undo() {
    if (g_hasCurrent && !g_current.pts.empty()) {
        g_current.pts.pop_back();
        if (g_current.pts.empty()) g_hasCurrent = false;
        return;
    }
    if (!g_shapes.empty()) {
        const Shape& last = g_shapes.back();
        int& counter = CounterFor(last.kind);
        if (counter == last.number) --counter;
        g_shapes.pop_back();
    }
}

void Clear() {
    g_shapes.clear();
    g_current = Shape();
    g_hasCurrent = false;
    g_counter[0] = g_counter[1] = g_counter[2] = 0;
    Log("tum cizimler silindi");
}

const std::vector<Shape>& Shapes() { return g_shapes; }

const Shape* Current() { return g_hasCurrent ? &g_current : nullptr; }

int ShapeCount() { return static_cast<int>(g_shapes.size()); }

}  // namespace Drawings
