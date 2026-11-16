#include "Geo.h"

#include <cmath>

namespace
{

const double kPi = 3.14159265358979323846;
const double kEarthRadiusNm = 3440.065;  // dunya yaricapi, deniz mili

double rad(double pDegrees)
{
    return pDegrees * kPi / 180.0;
}

double deg(double pRadians)
{
    return pRadians * 180.0 / kPi;
}

}  // namespace

double distanceNm(const LatLon& pFrom, const LatLon& pTo)
{
    const double tDeltaLat = rad(pTo.mLat - pFrom.mLat);
    const double tDeltaLon = rad(pTo.mLon - pFrom.mLon);
    const double tH = std::sin(tDeltaLat / 2) * std::sin(tDeltaLat / 2) +
                      std::cos(rad(pFrom.mLat)) * std::cos(rad(pTo.mLat)) * std::sin(tDeltaLon / 2) *
                          std::sin(tDeltaLon / 2);
    return 2.0 * kEarthRadiusNm * std::asin(std::sqrt(tH < 1.0 ? tH : 1.0));
}

LatLon greatCircleInterpolate(const LatLon& pFrom, const LatLon& pTo, double pFraction)
{
    const double tLat1 = rad(pFrom.mLat);
    const double tLon1 = rad(pFrom.mLon);
    const double tLat2 = rad(pTo.mLat);
    const double tLon2 = rad(pTo.mLon);
    const double tX1 = std::cos(tLat1) * std::cos(tLon1);
    const double tY1 = std::cos(tLat1) * std::sin(tLon1);
    const double tZ1 = std::sin(tLat1);
    const double tX2 = std::cos(tLat2) * std::cos(tLon2);
    const double tY2 = std::cos(tLat2) * std::sin(tLon2);
    const double tZ2 = std::sin(tLat2);

    double tDot = tX1 * tX2 + tY1 * tY2 + tZ1 * tZ2;
    if (tDot > 1.0)
    {
        tDot = 1.0;
    }
    if (tDot < -1.0)
    {
        tDot = -1.0;
    }
    const double tAngle = std::acos(tDot);
    if (tAngle < 1e-12)
    {
        return pFrom;
    }

    const double tSin = std::sin(tAngle);
    const double tWeightFrom = std::sin((1 - pFraction) * tAngle) / tSin;
    const double tWeightTo = std::sin(pFraction * tAngle) / tSin;
    const double tX = tWeightFrom * tX1 + tWeightTo * tX2;
    const double tY = tWeightFrom * tY1 + tWeightTo * tY2;
    const double tZ = tWeightFrom * tZ1 + tWeightTo * tZ2;

    LatLon tResult;
    tResult.mLat = deg(std::atan2(tZ, std::sqrt(tX * tX + tY * tY)));
    tResult.mLon = deg(std::atan2(tY, tX));
    return tResult;
}

LatLon destinationPoint(const LatLon& pFrom, double pBearingDeg, double pDistanceNm)
{
    const double tLat1 = rad(pFrom.mLat);
    const double tLon1 = rad(pFrom.mLon);
    const double tBearing = rad(pBearingDeg);
    const double tAngle = pDistanceNm / kEarthRadiusNm;  // kat edilen yay (radyan)

    const double tLat2 = std::asin(std::sin(tLat1) * std::cos(tAngle) +
                                   std::cos(tLat1) * std::sin(tAngle) * std::cos(tBearing));
    const double tLon2 = tLon1 + std::atan2(std::sin(tBearing) * std::sin(tAngle) * std::cos(tLat1),
                                            std::cos(tAngle) - std::sin(tLat1) * std::sin(tLat2));
    LatLon tResult;
    tResult.mLat = deg(tLat2);
    tResult.mLon = deg(tLon2);
    return tResult;
}

double initialBearingDeg(const LatLon& pFrom, const LatLon& pTo)
{
    const double tLat1 = rad(pFrom.mLat);
    const double tLat2 = rad(pTo.mLat);
    const double tDeltaLon = rad(pTo.mLon - pFrom.mLon);
    const double tY = std::sin(tDeltaLon) * std::cos(tLat2);
    const double tX = std::cos(tLat1) * std::sin(tLat2) - std::sin(tLat1) * std::cos(tLat2) * std::cos(tDeltaLon);
    const double tBearing = deg(std::atan2(tY, tX));
    return tBearing < 0.0 ? tBearing + 360.0 : tBearing;
}

double angleDifferenceDeg(double pFirst, double pSecond)
{
    double tDifference = std::fmod(std::fabs(pFirst - pSecond), 360.0);
    if (tDifference > 180.0)
    {
        tDifference = 360.0 - tDifference;
    }
    return tDifference;
}

void densifySegment(const LatLon& pFrom, const LatLon& pTo, double pMaxStepNm, std::vector<LatLon>& pOut)
{
    pOut.push_back(pFrom);
    const double tDistance = distanceNm(pFrom, pTo);
    if (pMaxStepNm <= 0 || tDistance <= pMaxStepNm)
    {
        return;
    }
    int tSteps = static_cast<int>(std::ceil(tDistance / pMaxStepNm));
    if (tSteps > 500)
    {
        tSteps = 500;  // asiri uzun segmentte sinir
    }
    for (int tStep = 1; tStep < tSteps; ++tStep)
    {
        pOut.push_back(greatCircleInterpolate(pFrom, pTo, double(tStep) / tSteps));
    }
}

// Ucgenleme: kulak kesme (ear clipping). Lat/lon kucuk alanda duz kabul:
// boylam cos(enlem) ile olceklenir.
namespace
{

struct Point2
{
    double mX;
    double mY;
};

double cross(const Point2& pOrigin, const Point2& pA, const Point2& pB)
{
    return (pA.mX - pOrigin.mX) * (pB.mY - pOrigin.mY) - (pA.mY - pOrigin.mY) * (pB.mX - pOrigin.mX);
}

bool pointInTriangle(const Point2& pPoint, const Point2& pA, const Point2& pB, const Point2& pC)
{
    const double tD1 = cross(pA, pB, pPoint);
    const double tD2 = cross(pB, pC, pPoint);
    const double tD3 = cross(pC, pA, pPoint);
    const bool tHasNegative = (tD1 < 0) || (tD2 < 0) || (tD3 < 0);
    const bool tHasPositive = (tD1 > 0) || (tD2 > 0) || (tD3 > 0);
    return !(tHasNegative && tHasPositive);
}

bool segmentsIntersect(const Point2& pA, const Point2& pB, const Point2& pC, const Point2& pD)
{
    const double tD1 = cross(pC, pD, pA);
    const double tD2 = cross(pC, pD, pB);
    const double tD3 = cross(pA, pB, pC);
    const double tD4 = cross(pA, pB, pD);
    return ((tD1 > 0) != (tD2 > 0)) && ((tD3 > 0) != (tD4 > 0)) && tD1 != 0 && tD2 != 0 && tD3 != 0 &&
           tD4 != 0;
}

}  // namespace

double polygonAreaNm2(const std::vector<LatLon>& pPolygon)
{
    const size_t tCount = pPolygon.size();
    if (tCount < 3)
    {
        return 0.0;
    }
    double tMeanLat = 0.0;
    for (const LatLon& tVertex : pPolygon)
    {
        tMeanLat += tVertex.mLat;
    }
    tMeanLat /= static_cast<double>(tCount);
    const double tLonScale = std::cos(rad(tMeanLat));

    // Shoelace formulu; 1 derece enlem = 60 NM
    double tSum = 0.0;
    for (size_t tIndex = 0; tIndex < tCount; ++tIndex)
    {
        const LatLon& tA = pPolygon[tIndex];
        const LatLon& tB = pPolygon[(tIndex + 1) % tCount];
        tSum += (tA.mLon * tLonScale) * tB.mLat - (tB.mLon * tLonScale) * tA.mLat;
    }
    return std::fabs(tSum) * 0.5 * 60.0 * 60.0;
}

bool triangulatePolygon(const std::vector<LatLon>& pPolygon, std::vector<int>& pOutTriangles)
{
    pOutTriangles.clear();
    const int tCount = static_cast<int>(pPolygon.size());
    if (tCount < 3)
    {
        return false;
    }

    double tMeanLat = 0;
    for (const LatLon& tVertex : pPolygon)
    {
        tMeanLat += tVertex.mLat;
    }
    tMeanLat /= tCount;
    const double tLonScale = std::cos(rad(tMeanLat));

    std::vector<Point2> tPoints(tCount);
    for (int tIndex = 0; tIndex < tCount; ++tIndex)
    {
        tPoints[tIndex] = {pPolygon[tIndex].mLon * tLonScale, pPolygon[tIndex].mLat};
    }

    for (int tI = 0; tI < tCount; ++tI)
    {
        for (int tJ = tI + 1; tJ < tCount; ++tJ)
        {
            if (tJ == tI + 1 || (tI == 0 && tJ == tCount - 1))
            {
                continue;  // komsu kenarlar
            }
            if (segmentsIntersect(tPoints[tI], tPoints[(tI + 1) % tCount], tPoints[tJ],
                                  tPoints[(tJ + 1) % tCount]))
            {
                return false;
            }
        }
    }

    // Yonu saat yonunun tersine (CCW) cevir
    double tArea2 = 0;
    for (int tIndex = 0; tIndex < tCount; ++tIndex)
    {
        const Point2& tA = tPoints[tIndex];
        const Point2& tB = tPoints[(tIndex + 1) % tCount];
        tArea2 += tA.mX * tB.mY - tB.mX * tA.mY;
    }
    if (std::fabs(tArea2) < 1e-14)
    {
        return false;  // alani yok (hepsi ayni dogru uzerinde)
    }

    std::vector<int> tOrder(tCount);
    for (int tIndex = 0; tIndex < tCount; ++tIndex)
    {
        tOrder[tIndex] = (tArea2 > 0) ? tIndex : (tCount - 1 - tIndex);
    }

    int tGuard = 0;
    while (tOrder.size() > 3 && tGuard < tCount * tCount)
    {
        ++tGuard;
        bool tClipped = false;
        const int tRemaining = static_cast<int>(tOrder.size());
        for (int tI = 0; tI < tRemaining; ++tI)
        {
            const int tIa = tOrder[(tI + tRemaining - 1) % tRemaining];
            const int tIb = tOrder[tI];
            const int tIc = tOrder[(tI + 1) % tRemaining];
            const Point2& tA = tPoints[tIa];
            const Point2& tB = tPoints[tIb];
            const Point2& tC = tPoints[tIc];
            if (cross(tA, tB, tC) <= 0)
            {
                continue;  // icbukey kose, kulak degil
            }
            bool tContains = false;
            for (int tJ = 0; tJ < tRemaining && !tContains; ++tJ)
            {
                const int tIj = tOrder[tJ];
                if (tIj == tIa || tIj == tIb || tIj == tIc)
                {
                    continue;
                }
                tContains = pointInTriangle(tPoints[tIj], tA, tB, tC);
            }
            if (tContains)
            {
                continue;
            }
            pOutTriangles.push_back(tIa);
            pOutTriangles.push_back(tIb);
            pOutTriangles.push_back(tIc);
            tOrder.erase(tOrder.begin() + tI);
            tClipped = true;
            break;
        }
        if (!tClipped)
        {
            // Kulak bulunamadi: ayni dogru uzerindeki (alansiz) bir kose varsa
            // onu atip devam et, yoksa basarisiz.
            bool tRemoved = false;
            for (int tI = 0; tI < tRemaining && !tRemoved; ++tI)
            {
                const Point2& tA = tPoints[tOrder[(tI + tRemaining - 1) % tRemaining]];
                const Point2& tB = tPoints[tOrder[tI]];
                const Point2& tC = tPoints[tOrder[(tI + 1) % tRemaining]];
                if (std::fabs(cross(tA, tB, tC)) < 1e-14)
                {
                    tOrder.erase(tOrder.begin() + tI);
                    tRemoved = true;
                }
            }
            if (!tRemoved)
            {
                pOutTriangles.clear();
                return false;
            }
        }
    }
    pOutTriangles.push_back(tOrder[0]);
    pOutTriangles.push_back(tOrder[1]);
    pOutTriangles.push_back(tOrder[2]);
    return true;
}
