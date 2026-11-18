#include "data/AirportSearch.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

#include "XMapView.h"
#include "data/AirportGeometryStore.h"

namespace
{
const int kNoMatch = 99;
const size_t kMinContainsLength = 3;
const double kFitFraction = 0.6;
const double kMinFitZoom = 10.0;
const double kMaxFitZoom = 15.0;
const double kDefaultFitZoom = 13.5;

char toUpper(char pChar)
{
    return (pChar >= 'a' && pChar <= 'z') ? static_cast<char>(pChar - 'a' + 'A') : pChar;
}

std::string upper(std::string_view pText)
{
    std::string tOut(pText);
    std::transform(tOut.begin(), tOut.end(), tOut.begin(), toUpper);
    return tOut;
}

bool startsWith(const std::string& pText, const std::string& pPrefix)
{
    return pText.compare(0, pPrefix.size(), pPrefix) == 0;
}

int matchRank(const AirportGeometry& pAirport, const std::string& pQuery)
{
    const std::string tId = upper(pAirport.mId);
    const std::string tIcao = upper(pAirport.mIcaoCode);
    if (tId == pQuery || (!tIcao.empty() && tIcao == pQuery))
    {
        return 0;
    }
    if (startsWith(tId, pQuery) || (!tIcao.empty() && startsWith(tIcao, pQuery)))
    {
        return 1;
    }
    const std::string tName = upper(pAirport.mName);
    const size_t tFound = tName.find(pQuery);
    if (tFound == std::string::npos)
    {
        return kNoMatch;
    }
    for (size_t tAt = tFound; tAt != std::string::npos; tAt = tName.find(pQuery, tAt + 1))
    {
        const bool tWordStart = tAt == 0 || !std::isalnum(static_cast<unsigned char>(tName[tAt - 1]));
        if (tWordStart)
        {
            return 2;
        }
    }
    return pQuery.size() >= kMinContainsLength ? 3 : kNoMatch;
}

double longestRunwayNm(const AirportGeometry& pAirport)
{
    double tLongest = 0.0;
    for (const Runway& tRunway : pAirport.mRunways)
    {
        tLongest = std::max(tLongest, distanceNm(tRunway.mEnds[0].mPosition, tRunway.mEnds[1].mPosition));
    }
    return tLongest;
}
}

std::vector<int> findAirports(const AirportGeometryStore& pStore, std::string_view pQuery, int pMaxResults)
{
    struct Candidate
    {
        int mIndex;
        int mRank;
        double mRunway;
    };
    std::vector<int> tResult;
    const std::string tQuery = upper(pQuery);
    if (tQuery.empty() || !pStore.isReady())
    {
        return tResult;
    }
    std::vector<Candidate> tCandidates;
    for (int tIndex = 0; tIndex < pStore.count(); ++tIndex)
    {
        const AirportGeometry& tAirport = pStore.at(tIndex);
        const int tRank = matchRank(tAirport, tQuery);
        if (tRank != kNoMatch)
        {
            tCandidates.push_back({tIndex, tRank, longestRunwayNm(tAirport)});
        }
    }
    const size_t tCount = std::min(tCandidates.size(), static_cast<size_t>(std::max(0, pMaxResults)));
    std::partial_sort(tCandidates.begin(), tCandidates.begin() + tCount, tCandidates.end(),
                      [&pStore](const Candidate& pFirst, const Candidate& pSecond)
                      {
                          if (pFirst.mRank != pSecond.mRank)
                          {
                              return pFirst.mRank < pSecond.mRank;
                          }
                          if (pFirst.mRunway != pSecond.mRunway)
                          {
                              return pFirst.mRunway > pSecond.mRunway;
                          }
                          return pStore.at(pFirst.mIndex).mId < pStore.at(pSecond.mIndex).mId;
                      });
    for (size_t tIndex = 0; tIndex < tCount; ++tIndex)
    {
        tResult.push_back(tCandidates[tIndex].mIndex);
    }
    return tResult;
}

LatLon airportCenter(const AirportGeometry& pAirport)
{
    const GeoBounds& tBounds = pAirport.mBounds;
    if (tBounds.mNorth > tBounds.mSouth && tBounds.mEast > tBounds.mWest)
    {
        return {(tBounds.mSouth + tBounds.mNorth) * 0.5, (tBounds.mWest + tBounds.mEast) * 0.5};
    }
    return pAirport.mReference;
}

double zoomToFitAirport(const AirportGeometry& pAirport, double pViewWidth, double pViewHeight)
{
    const GeoBounds& tBounds = pAirport.mBounds;
    const double tWidth = Mercator::lonToX(tBounds.mEast) - Mercator::lonToX(tBounds.mWest);
    const double tHeight = Mercator::latToY(tBounds.mNorth) - Mercator::latToY(tBounds.mSouth);
    if (tWidth <= 0.0 || tHeight <= 0.0 || pViewWidth <= 0.0 || pViewHeight <= 0.0)
    {
        return kDefaultFitZoom;
    }
    const double tZoomX = std::log2(pViewWidth * kFitFraction / (tWidth * 256.0));
    const double tZoomY = std::log2(pViewHeight * kFitFraction / (tHeight * 256.0));
    return std::clamp(std::min(tZoomX, tZoomY), kMinFitZoom, kMaxFitZoom);
}
