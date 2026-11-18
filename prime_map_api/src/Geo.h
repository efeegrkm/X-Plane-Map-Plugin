// Cografi hesaplar: mesafe, yon, buyuk daire ve hedef nokta.
#ifndef PRIME_MAP_SRC_GEO_H
#define PRIME_MAP_SRC_GEO_H

#include <vector>

struct LatLon
{
    double mLat;
    double mLon;
};

double distanceNm(const LatLon& pFrom, const LatLon& pTo);

LatLon greatCircleInterpolate(const LatLon& pFrom, const LatLon& pTo, double pFraction);

LatLon destinationPoint(const LatLon& pFrom, double pBearingDeg, double pDistanceNm);

double initialBearingDeg(const LatLon& pFrom, const LatLon& pTo);

double angleDifferenceDeg(double pFirst, double pSecond);

void densifySegment(const LatLon& pFrom, const LatLon& pTo, double pMaxStepNm, std::vector<LatLon>& pOut);

double polygonAreaNm2(const std::vector<LatLon>& pPolygon);

bool triangulatePolygon(const std::vector<LatLon>& pPolygon, std::vector<int>& pOutTriangles);
#endif
