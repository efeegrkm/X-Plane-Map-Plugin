// Geo.cpp
#include "Geo.h"

#include <cmath>

static const double kPi = 3.14159265358979323846;
static const double kEarthRadiusNm = 3440.065;  // dunya yaricapi, deniz mili

static double Rad(double deg) { return deg * kPi / 180.0; }
static double Deg(double rad) { return rad * 180.0 / kPi; }

double DistanceNm(const LatLon& a, const LatLon& b) {
    // Haversine formulu
    double dLat = Rad(b.lat - a.lat);
    double dLon = Rad(b.lon - a.lon);
    double h = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(Rad(a.lat)) * std::cos(Rad(b.lat)) *
               std::sin(dLon / 2) * std::sin(dLon / 2);
    return 2.0 * kEarthRadiusNm * std::asin(std::sqrt(h < 1.0 ? h : 1.0));
}

LatLon GreatCircleInterpolate(const LatLon& a, const LatLon& b, double f) {
    // Iki noktayi 3B birim vektore cevirip kuresel dogrusal ara deger (slerp)
    double lat1 = Rad(a.lat), lon1 = Rad(a.lon);
    double lat2 = Rad(b.lat), lon2 = Rad(b.lon);
    double x1 = std::cos(lat1) * std::cos(lon1), y1 = std::cos(lat1) * std::sin(lon1), z1 = std::sin(lat1);
    double x2 = std::cos(lat2) * std::cos(lon2), y2 = std::cos(lat2) * std::sin(lon2), z2 = std::sin(lat2);

    double dot = x1 * x2 + y1 * y2 + z1 * z2;
    if (dot > 1.0) dot = 1.0;
    if (dot < -1.0) dot = -1.0;
    double angle = std::acos(dot);
    if (angle < 1e-12) return a;  // ayni nokta

    double s = std::sin(angle);
    double wa = std::sin((1 - f) * angle) / s;
    double wb = std::sin(f * angle) / s;
    double x = wa * x1 + wb * x2, y = wa * y1 + wb * y2, z = wa * z1 + wb * z2;

    LatLon r;
    r.lat = Deg(std::atan2(z, std::sqrt(x * x + y * y)));
    r.lon = Deg(std::atan2(y, x));
    return r;
}

void DensifySegment(const LatLon& a, const LatLon& b, double maxStepNm,
                    std::vector<LatLon>& out) {
    out.push_back(a);
    double d = DistanceNm(a, b);
    if (maxStepNm <= 0 || d <= maxStepNm) return;
    int steps = static_cast<int>(std::ceil(d / maxStepNm));
    if (steps > 500) steps = 500;  // asiri uzun segmentte sinir
    for (int i = 1; i < steps; ++i) {
        out.push_back(GreatCircleInterpolate(a, b, double(i) / steps));
    }
}

// ---------------------------------------------------------------------------
// Ucgenleme: "kulak kesme" (ear clipping) yontemi.
// Lat/lon'u kucuk bir alanda duz kabul ediyoruz; boylami cos(enlem) ile
// olcekleyerek sekli bozmadan duzleme yaziyoruz.

namespace {
struct P2 { double x, y; };

double Cross(const P2& o, const P2& a, const P2& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

bool PointInTriangle(const P2& p, const P2& a, const P2& b, const P2& c) {
    double d1 = Cross(a, b, p), d2 = Cross(b, c, p), d3 = Cross(c, a, p);
    bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(hasNeg && hasPos);
}

bool SegmentsIntersect(const P2& a, const P2& b, const P2& c, const P2& d) {
    double d1 = Cross(c, d, a), d2 = Cross(c, d, b);
    double d3 = Cross(a, b, c), d4 = Cross(a, b, d);
    return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0)) &&
           d1 != 0 && d2 != 0 && d3 != 0 && d4 != 0;
}
}  // namespace

bool TriangulatePolygon(const std::vector<LatLon>& poly, std::vector<int>& outTriangles) {
    outTriangles.clear();
    const int n = static_cast<int>(poly.size());
    if (n < 3) return false;

    double meanLat = 0;
    for (const auto& p : poly) meanLat += p.lat;
    meanLat /= n;
    double k = std::cos(Rad(meanLat));

    std::vector<P2> pts(n);
    for (int i = 0; i < n; ++i) pts[i] = {poly[i].lon * k, poly[i].lat};

    // Kendini kesen poligonu reddet (kenar kenar kontrol)
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (j == i + 1 || (i == 0 && j == n - 1)) continue;  // komsu kenarlar
            if (SegmentsIntersect(pts[i], pts[(i + 1) % n], pts[j], pts[(j + 1) % n])) return false;
        }
    }

    // Yonu saat yonunun tersine (CCW) cevir
    double area2 = 0;
    for (int i = 0; i < n; ++i) {
        const P2& a = pts[i];
        const P2& b = pts[(i + 1) % n];
        area2 += a.x * b.y - b.x * a.y;
    }
    if (std::fabs(area2) < 1e-14) return false;  // alani yok (hepsi ayni dogru uzerinde)

    std::vector<int> idx(n);
    for (int i = 0; i < n; ++i) idx[i] = (area2 > 0) ? i : (n - 1 - i);

    int guard = 0;
    while (idx.size() > 3 && guard < n * n) {
        ++guard;
        bool clipped = false;
        const int m = static_cast<int>(idx.size());
        for (int i = 0; i < m; ++i) {
            int ia = idx[(i + m - 1) % m], ib = idx[i], ic = idx[(i + 1) % m];
            const P2 &a = pts[ia], &b = pts[ib], &c = pts[ic];
            if (Cross(a, b, c) <= 0) continue;  // icbukey kose, kulak degil
            bool contains = false;
            for (int j = 0; j < m && !contains; ++j) {
                int ij = idx[j];
                if (ij == ia || ij == ib || ij == ic) continue;
                contains = PointInTriangle(pts[ij], a, b, c);
            }
            if (contains) continue;
            outTriangles.push_back(ia);
            outTriangles.push_back(ib);
            outTriangles.push_back(ic);
            idx.erase(idx.begin() + i);
            clipped = true;
            break;
        }
        if (!clipped) {
            // Kulak bulunamadi: ayni dogru uzerindeki (alansiz) bir kose varsa
            // onu atip devam et, yoksa basarisiz.
            bool removed = false;
            for (int i = 0; i < m && !removed; ++i) {
                const P2& a = pts[idx[(i + m - 1) % m]];
                const P2& b = pts[idx[i]];
                const P2& c = pts[idx[(i + 1) % m]];
                if (std::fabs(Cross(a, b, c)) < 1e-14) {
                    idx.erase(idx.begin() + i);
                    removed = true;
                }
            }
            if (!removed) {
                outTriangles.clear();
                return false;
            }
        }
    }
    outTriangles.push_back(idx[0]);
    outTriangles.push_back(idx[1]);
    outTriangles.push_back(idx[2]);
    return true;
}
