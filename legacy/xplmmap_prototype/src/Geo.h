// Geo.h
// Cografi yardimcilar. X-Plane'e bagimli DEGIL, saf hesap:
//   - iki nokta arasi mesafe (deniz mili)
//   - buyuk daire (great circle) uzerinde ara nokta uretme
//   - bir bolgeyi (poligonu) ucgenlere bolme (dolgu cizimi icin)
#pragma once

#include <vector>

struct LatLon {
    double lat;  // derece, kuzey +
    double lon;  // derece, dogu +
};

// Iki nokta arasi buyuk daire mesafesi, deniz mili (NM)
double DistanceNm(const LatLon& a, const LatLon& b);

// a ile b arasinda, buyuk daire uzerinde f oraninda (0..1) nokta
LatLon GreatCircleInterpolate(const LatLon& a, const LatLon& b, double f);

// a -> b dogrusunu haritada dogru egriyle cizebilmek icin ara noktalar uretir.
// "out" sonuna a'yi ve maxStepNm'den uzun araliklarla ara noktalari ekler
// (b'yi EKLEMEZ; bir sonraki segmentin ilk noktasi olarak eklenir).
void DensifySegment(const LatLon& a, const LatLon& b, double maxStepNm,
                    std::vector<LatLon>& out);

// Basit bir poligonu (kendini kesmeyen) ucgenlere boler.
// Sonuc: outTriangles icinde 3'erli indeks gruplari (poly'deki sirayla).
// Basarisizsa (kendini kesen poligon vb.) false doner.
bool TriangulatePolygon(const std::vector<LatLon>& poly, std::vector<int>& outTriangles);
