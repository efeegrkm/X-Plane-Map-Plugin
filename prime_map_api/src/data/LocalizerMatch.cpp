// LocalizerMatch.cpp
#include "data/LocalizerMatch.h"

#include <cmath>
#include <vector>

#include "data/AirportGeometryStore.h"

namespace
{

const double kSearchDeg = 0.06;          // anten cevresinde aranacak alan (~6 km)
const double kMaxCourseDiff = 6.0;       // pist inis yonu ile localizer yonu farki (derece)
const double kMaxAntennaToEndNm = 1.6;   // anten ile pistin karsi ucu arasi (~3 km)
const double kMaxCrossTrackNm = 0.15;    // antenin pist ekseninden yana kaymasi (~280 m)
const double kPi = 3.14159265358979323846;

}  // namespace

bool FindLocalizerThreshold(const LatLon& pAntenna, double pCourseTrue, const AirportGeometryStore& pRunways,
                            LatLon* pOutThreshold)
{
    if (!pRunways.IsReady())
    {
        return false;
    }
    GeoBounds tBounds;
    tBounds.mSouth = pAntenna.mLat - kSearchDeg;
    tBounds.mNorth = pAntenna.mLat + kSearchDeg;
    tBounds.mWest = pAntenna.mLon - kSearchDeg;
    tBounds.mEast = pAntenna.mLon + kSearchDeg;
    std::vector<int> tAirports;
    pRunways.Query(tBounds, tAirports);

    bool tFound = false;
    double tBestDistance = 1e9;
    for (int tAirportIndex : tAirports)
    {
        for (const Runway& tRunway : pRunways.At(tAirportIndex).mRunways)
        {
            // Her pistin iki inis yonu var: uc 0'a inip uc 1'e dogru, ya da tersi
            for (int tLandingEnd = 0; tLandingEnd < 2; ++tLandingEnd)
            {
                const LatLon& tThreshold = tRunway.mEnds[tLandingEnd].mPosition;
                const LatLon& tFarEnd = tRunway.mEnds[1 - tLandingEnd].mPosition;
                const double tLandingCourse = InitialBearingDeg(tThreshold, tFarEnd);
                if (AngleDifferenceDeg(tLandingCourse, pCourseTrue) > kMaxCourseDiff)
                {
                    continue;
                }
                const double tToEnd = DistanceNm(tFarEnd, pAntenna);
                if (tToEnd > kMaxAntennaToEndNm)
                {
                    continue;
                }
                // Eksenden yana kayma: esikten antene olan yonun pist yonune gore acisi
                const double tFromThreshold = DistanceNm(tThreshold, pAntenna);
                const double tOffAngle =
                    AngleDifferenceDeg(InitialBearingDeg(tThreshold, pAntenna), tLandingCourse) * kPi / 180.0;
                if (tFromThreshold * std::sin(tOffAngle) > kMaxCrossTrackNm)
                {
                    continue;
                }
                if (tToEnd < tBestDistance)
                {
                    tBestDistance = tToEnd;
                    *pOutThreshold = tThreshold;
                    tFound = true;
                }
            }
        }
    }
    return tFound;
}
