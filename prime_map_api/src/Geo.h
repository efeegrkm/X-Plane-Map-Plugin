// Cografi yardimcilar. X-Plane'e bagimli DEGIL, saf hesap (mesafe, buyuk daire, ucgenleme).
#pragma once

#include <vector>

struct LatLon
{
    double mLat;  // derece, kuzey +
    double mLon;  // derece, dogu +
};

// deniz mili (NM)
double distanceNm(const LatLon& pFrom, const LatLon& pTo);

// buyuk daire uzerinde pFraction oraninda (0..1) nokta
LatLon greatCircleInterpolate(const LatLon& pFrom, const LatLon& pTo, double pFraction);

// pBearingDeg: gercek yon, 0 = kuzey, saat yonunde; pDistanceNm: deniz mili
LatLon destinationPoint(const LatLon& pFrom, double pBearingDeg, double pDistanceNm);

// gercek yon, 0..360, 0 = kuzey
double initialBearingDeg(const LatLon& pFrom, const LatLon& pTo);

// (0..180)
double angleDifferenceDeg(double pFirst, double pSecond);

// pTo'yu EKLEMEZ (sonraki segmentin ilk noktasi); araliklar pMaxStepNm'yi gecmez
void densifySegment(const LatLon& pFrom, const LatLon& pTo, double pMaxStepNm, std::vector<LatLon>& pOut);

// yaklasik alan, NM^2. Kucuk alanlarda duz kabul: boylam cos(ortalama enlem) ile olceklenir.
double polygonAreaNm2(const std::vector<LatLon>& pPolygon);

// 3'erli indeks gruplari uretir; kendini kesen poligonda false
bool triangulatePolygon(const std::vector<LatLon>& pPolygon, std::vector<int>& pOutTriangles);
