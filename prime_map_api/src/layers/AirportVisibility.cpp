#include "layers/AirportVisibility.h"

#include <algorithm>
#include <cmath>

#include "XMapView.h"
#include "data/AirportGeometryStore.h"

namespace
{
const double kQueryMarginDeg = 0.05;
}

void findVisibleAirports(const XMapView& pView, const AirportGeometryStore& pStore, std::vector<int>& pScratch,
                         std::vector<int>& pOut, bool pNeedNodes)
{
    pOut.clear();
    if (!pStore.isReady())
    {
        return;
    }
    const GeoBounds tScreen = pView.visibleBounds();
    GeoBounds tQuery = tScreen;
    tQuery.mSouth -= kQueryMarginDeg;
    tQuery.mNorth += kQueryMarginDeg;
    tQuery.mWest -= kQueryMarginDeg;
    tQuery.mEast += kQueryMarginDeg;
    pScratch.clear();
    pStore.query(tQuery, pScratch);
    for (int tIndex : pScratch)
    {
        const AirportGeometry& tAirport = pStore.at(tIndex);
        const GeoBounds& tBox = tAirport.mBounds;
        if ((pNeedNodes && tAirport.mNodeCount == 0) || tBox.mNorth < tScreen.mSouth || tBox.mSouth > tScreen.mNorth ||
            tBox.mEast < tScreen.mWest || tBox.mWest > tScreen.mEast)
        {
            continue;
        }
        pOut.push_back(tIndex);
    }
    const LatLon tCenter = pView.center();
    std::sort(pOut.begin(), pOut.end(), [&](int pA, int pB)
              {
                  const LatLon& tA = pStore.at(pA).mReference;
                  const LatLon& tB = pStore.at(pB).mReference;
                  return std::hypot(tA.mLat - tCenter.mLat, tA.mLon - tCenter.mLon) <
                         std::hypot(tB.mLat - tCenter.mLat, tB.mLon - tCenter.mLon);
              });
}
