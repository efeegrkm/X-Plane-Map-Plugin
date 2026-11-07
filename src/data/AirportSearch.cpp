// AirportSearch.cpp
#include "data/AirportSearch.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

#include "MapView.h"
#include "data/AirportGeometryStore.h"

namespace
{

const int kNoMatch = 99;
const size_t kMinContainsLength = 3;  // "ad icinde gecer" eslesmesi icin en az bu kadar harf
const double kFitFraction = 0.6;      // havalimani pencerenin bu kadarini kaplasin
const double kMinFitZoom = 10.0;
const double kMaxFitZoom = 15.0;
const double kDefaultFitZoom = 13.5;  // kapsayan kutusu olmayan (helipad) havalimani

char ToUpper(char pChar)
{
    return (pChar >= 'a' && pChar <= 'z') ? static_cast<char>(pChar - 'a' + 'A') : pChar;
}

std::string Upper(std::string_view pText)
{
    std::string tOut(pText);
    std::transform(tOut.begin(), tOut.end(), tOut.begin(), ToUpper);
    return tOut;
}

bool StartsWith(const std::string& pText, const std::string& pPrefix)
{
    return pText.compare(0, pPrefix.size(), pPrefix) == 0;
}

// Eslesme derecesi (kucuk = iyi), eslesmezse kNoMatch. pQuery buyuk harfli.
int MatchRank(const AirportGeometry& pAirport, const std::string& pQuery)
{
    const std::string tId = Upper(pAirport.mId);
    const std::string tIcao = Upper(pAirport.mIcaoCode);
    if (tId == pQuery || (!tIcao.empty() && tIcao == pQuery))
    {
        return 0;
    }
    if (StartsWith(tId, pQuery) || (!tIcao.empty() && StartsWith(tIcao, pQuery)))
    {
        return 1;
    }
    const std::string tName = Upper(pAirport.mName);
    const size_t tFound = tName.find(pQuery);
    if (tFound == std::string::npos)
    {
        return kNoMatch;
    }
    // Kelime basi mi? (ilk karakter ya da oncesi harf/rakam degil)
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

// Ayni derecede siralama: buyuk havalimani once
double LongestRunwayNm(const AirportGeometry& pAirport)
{
    double tLongest = 0.0;
    for (const Runway& tRunway : pAirport.mRunways)
    {
        tLongest = std::max(tLongest, DistanceNm(tRunway.mEnds[0].mPosition, tRunway.mEnds[1].mPosition));
    }
    return tLongest;
}

}  // namespace

std::vector<int> FindAirports(const AirportGeometryStore& pStore, std::string_view pQuery, int pMaxResults)
{
    struct Candidate
    {
        int mIndex;
        int mRank;
        double mRunway;
    };
    std::vector<int> tResult;
    const std::string tQuery = Upper(pQuery);
    if (tQuery.empty() || !pStore.IsReady())
    {
        return tResult;
    }
    std::vector<Candidate> tCandidates;
    for (int tIndex = 0; tIndex < pStore.Count(); ++tIndex)
    {
        const AirportGeometry& tAirport = pStore.At(tIndex);
        const int tRank = MatchRank(tAirport, tQuery);
        if (tRank != kNoMatch)
        {
            tCandidates.push_back({tIndex, tRank, LongestRunwayNm(tAirport)});
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
                          return pStore.At(pFirst.mIndex).mId < pStore.At(pSecond.mIndex).mId;
                      });
    for (size_t tIndex = 0; tIndex < tCount; ++tIndex)
    {
        tResult.push_back(tCandidates[tIndex].mIndex);
    }
    return tResult;
}

LatLon AirportCenter(const AirportGeometry& pAirport)
{
    const GeoBounds& tBounds = pAirport.mBounds;
    if (tBounds.mNorth > tBounds.mSouth && tBounds.mEast > tBounds.mWest)
    {
        return {(tBounds.mSouth + tBounds.mNorth) * 0.5, (tBounds.mWest + tBounds.mEast) * 0.5};
    }
    return pAirport.mReference;
}

double ZoomToFitAirport(const AirportGeometry& pAirport, double pViewWidth, double pViewHeight)
{
    const GeoBounds& tBounds = pAirport.mBounds;
    // Mercator birimi (0..1) cinsinden kutu; dunya kenari = 256 * 2^zoom boxel
    const double tWidth = Mercator::LonToX(tBounds.mEast) - Mercator::LonToX(tBounds.mWest);
    const double tHeight = Mercator::LatToY(tBounds.mNorth) - Mercator::LatToY(tBounds.mSouth);
    if (tWidth <= 0.0 || tHeight <= 0.0 || pViewWidth <= 0.0 || pViewHeight <= 0.0)
    {
        return kDefaultFitZoom;
    }
    const double tZoomX = std::log2(pViewWidth * kFitFraction / (tWidth * 256.0));
    const double tZoomY = std::log2(pViewHeight * kFitFraction / (tHeight * 256.0));
    return std::clamp(std::min(tZoomX, tZoomY), kMinFitZoom, kMaxFitZoom);
}
