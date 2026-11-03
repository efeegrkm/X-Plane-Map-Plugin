// Geo.h
// Cografi yardimcilar. X-Plane'e bagimli DEGIL, saf hesap:
//   - iki nokta arasi mesafe (deniz mili)
//   - buyuk daire (great circle) uzerinde ara nokta uretme
//   - bir bolgeyi (poligonu) ucgenlere bolme (dolgu cizimi icin)
#pragma once

#include <vector>

struct LatLon
{
    double mLat;  // derece, kuzey +
    double mLon;  // derece, dogu +
};

// Iki nokta arasi buyuk daire mesafesi, deniz mili (NM)
double DistanceNm(const LatLon& pFrom, const LatLon& pTo);

// pFrom ile pTo arasinda, buyuk daire uzerinde pFraction oraninda (0..1) nokta
LatLon GreatCircleInterpolate(const LatLon& pFrom, const LatLon& pTo, double pFraction);

// pFrom'dan pBearingDeg yonunde (gercek, saat yonunde, 0 = kuzey) pDistanceNm
// deniz mili gidince varilan nokta (buyuk daire)
LatLon DestinationPoint(const LatLon& pFrom, double pBearingDeg, double pDistanceNm);

// pFrom -> pTo dogrusunu haritada dogru egriyle cizebilmek icin ara noktalar uretir.
// pOut sonuna pFrom'u ve pMaxStepNm'den uzun araliklarla ara noktalari ekler
// (pTo'yu EKLEMEZ; bir sonraki segmentin ilk noktasi olarak eklenir).
void DensifySegment(const LatLon& pFrom, const LatLon& pTo, double pMaxStepNm, std::vector<LatLon>& pOut);

// Basit bir poligonu (kendini kesmeyen) ucgenlere boler.
// Sonuc: pOutTriangles icinde 3'erli indeks gruplari (pPolygon'daki sirayla).
// Basarisizsa (kendini kesen poligon vb.) false doner.
bool TriangulatePolygon(const std::vector<LatLon>& pPolygon, std::vector<int>& pOutTriangles);
