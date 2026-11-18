#include "data/LocalizerMatch.h"

#include <cmath>
#include <vector>

#include "data/AirportGeometryStore.h"

namespace
{
const double kSearchDeg = 0.06;
const double kMaxCourseDiff = 6.0;
const double kMaxAntennaToEndNm = 1.6;
const double kMaxCrossTrackNm = 0.15;
const double kPi = 3.14159265358979323846;
}

bool findLocalizerThreshold(const LatLon& pAntenna, double pCourseTrue, const AirportGeometryStore& pRunways,
                            LatLon* pOutThreshold)
{
    if (!pRunways.isReady())
    {
        return false;
    }
    GeoBounds tBounds;
    tBounds.mSouth = pAntenna.mLat - kSearchDeg;
    tBounds.mNorth = pAntenna.mLat + kSearchDeg;
    tBounds.mWest = pAntenna.mLon - kSearchDeg;
    tBounds.mEast = pAntenna.mLon + kSearchDeg;
    std::vector<int> tAirports;
    pRunways.query(tBounds, tAirports);

    bool tFound = false;
    double tBestDistance = 1e9;
    for (int tAirportIndex : tAirports)
    {
        for (const Runway& tRunway : pRunways.at(tAirportIndex).mRunways)
        {
            for (int tLandingEnd = 0; tLandingEnd < 2; ++tLandingEnd)
            {
                const LatLon& tThreshold = tRunway.mEnds[tLandingEnd].mPosition;
                const LatLon& tFarEnd = tRunway.mEnds[1 - tLandingEnd].mPosition;
                const double tLandingCourse = initialBearingDeg(tThreshold, tFarEnd);
                if (angleDifferenceDeg(tLandingCourse, pCourseTrue) > kMaxCourseDiff)
                {
                    continue;
                }
                const double tToEnd = distanceNm(tFarEnd, pAntenna);
                if (tToEnd > kMaxAntennaToEndNm)
                {
                    continue;
                }
                const double tFromThreshold = distanceNm(tThreshold, pAntenna);
                const double tOffAngle =
                    angleDifferenceDeg(initialBearingDeg(tThreshold, pAntenna), tLandingCourse) * kPi / 180.0;
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
