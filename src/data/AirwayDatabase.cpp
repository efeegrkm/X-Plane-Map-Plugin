// AirwayDatabase.cpp
#include "data/AirwayDatabase.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <locale>
#include <sstream>
#include <unordered_map>

#include "data/NavDatabase.h"

namespace
{

const int kTypeNdb = 2;
const int kTypeVhf = 3;
const int kTypeFix = 11;
const int kMaxCellsPerSegment = 400;  // cok uzun segmentler (okyanus asiri) icin ust sinir

// Nav kaydinin hava yolu dosyasindaki tur koduna uyup uymadigi
bool KindMatches(NavKind pKind, int pType)
{
    switch (pType)
    {
        case kTypeFix:
            return pKind == NavKind::Fix;
        case kTypeNdb:
            return pKind == NavKind::Ndb;
        case kTypeVhf:
            return pKind == NavKind::Vor || pKind == NavKind::Dme;
        default:
            return false;
    }
}

// Kimlik -> nav kayitlari (fix, NDB, VOR, DME); uc nokta aramak icin
class EndpointIndex
{
public:
    explicit EndpointIndex(const NavDatabase& pNav)
        : mNav(pNav)
    {
        for (int tIndex = 0; tIndex < pNav.Count(); ++tIndex)
        {
            const NavKind tKind = pNav.At(tIndex).mKind;
            if (tKind == NavKind::Fix || tKind == NavKind::Ndb || tKind == NavKind::Vor || tKind == NavKind::Dme)
            {
                mById.emplace(pNav.At(tIndex).mId, tIndex);
            }
        }
    }

    // Iki ucun adaylari arasindan birbirine en yakin cifti secer
    bool Resolve(const AirwayLine& pLine, LatLon* pOutFrom, LatLon* pOutTo) const
    {
        const auto tFromRange = mById.equal_range(pLine.mFromId);
        const auto tToRange = mById.equal_range(pLine.mToId);
        double tBest = 1e18;
        bool tFound = false;
        for (auto tFrom = tFromRange.first; tFrom != tFromRange.second; ++tFrom)
        {
            const NavRecord& tFromRecord = mNav.At(tFrom->second);
            if (!KindMatches(tFromRecord.mKind, pLine.mFromType))
            {
                continue;
            }
            for (auto tTo = tToRange.first; tTo != tToRange.second; ++tTo)
            {
                const NavRecord& tToRecord = mNav.At(tTo->second);
                if (!KindMatches(tToRecord.mKind, pLine.mToType))
                {
                    continue;
                }
                const LatLon tA = {tFromRecord.mLat, tFromRecord.mLon};
                const LatLon tB = {tToRecord.mLat, tToRecord.mLon};
                const double tDistance = DistanceNm(tA, tB);
                if (tDistance < tBest)
                {
                    tBest = tDistance;
                    *pOutFrom = tA;
                    *pOutTo = tB;
                    tFound = true;
                }
            }
        }
        return tFound;
    }

private:
    const NavDatabase& mNav;
    std::unordered_multimap<std::string, int> mById;
};

}  // namespace

bool AirwayDatabase::ParseLine(const std::string& pLine, AirwayLine* pOut)
{
    std::istringstream tStream(pLine);
    tStream.imbue(std::locale::classic());
    std::string tFromRegion;
    std::string tToRegion;
    std::string tDirection;
    if (!(tStream >> pOut->mFromId >> tFromRegion >> pOut->mFromType >> pOut->mToId >> tToRegion >> pOut->mToType >>
          tDirection >> pOut->mLevel >> pOut->mBaseFl >> pOut->mTopFl >> pOut->mNames))
    {
        return false;
    }
    if (tDirection.size() != 1 || (tDirection[0] != 'N' && tDirection[0] != 'F' && tDirection[0] != 'B'))
    {
        return false;
    }
    pOut->mDirection = tDirection[0];
    return pOut->mLevel == 1 || pOut->mLevel == 2;
}

bool AirwayDatabase::LoadFile(const std::string& pPath, const NavDatabase& pNav, const std::atomic<bool>& pCancel,
                              std::string* pReport)
{
    std::ifstream tFile(pPath);
    if (!tFile)
    {
        return false;
    }
    const EndpointIndex tIndex(pNav);
    mSegments.clear();
    int tLines = 0;
    int tUnresolved = 0;
    std::string tLine;
    AirwayLine tParsed;
    while (std::getline(tFile, tLine))
    {
        if ((tLines & 0x3FFF) == 0 && pCancel.load())
        {
            return false;
        }
        if (!ParseLine(tLine, &tParsed))
        {
            continue;
        }
        ++tLines;
        AirwaySegment tSegment;
        if (!tIndex.Resolve(tParsed, &tSegment.mFrom, &tSegment.mTo))
        {
            ++tUnresolved;  // uc nokta nav verisinde yok (farkli AIRAC dongusu vb.)
            continue;
        }
        tSegment.mHigh = tParsed.mLevel == 2;
        tSegment.mDirection = tParsed.mDirection;
        tSegment.mBaseFl = tParsed.mBaseFl;
        tSegment.mTopFl = tParsed.mTopFl;
        tSegment.mNames = tParsed.mNames;
        mSegments.push_back(tSegment);
    }
    BuildGrid();
    char tText[512];
    std::snprintf(tText, sizeof(tText), "%s: %d segments, %d unresolved endpoints", pPath.c_str(),
                  static_cast<int>(mSegments.size()), tUnresolved);
    *pReport = tText;
    return true;
}

void AirwayDatabase::BuildGrid()
{
    // Her segmenti, kapsayan kutusunun degdigi her 1x1 derece hucreye koy
    std::vector<GridItem> tItems;
    for (int tIndex = 0; tIndex < Count(); ++tIndex)
    {
        const AirwaySegment& tSegment = mSegments[tIndex];
        double tWest = std::fmin(tSegment.mFrom.mLon, tSegment.mTo.mLon);
        double tEast = std::fmax(tSegment.mFrom.mLon, tSegment.mTo.mLon);
        if (tEast - tWest > 180.0)
        {
            // 180. boylami geciyor: iki ucun hucresine koymak yeterli (nadir)
            tItems.push_back({static_cast<uint32_t>(tIndex), static_cast<float>(tSegment.mFrom.mLat),
                              static_cast<float>(tSegment.mFrom.mLon)});
            tItems.push_back({static_cast<uint32_t>(tIndex), static_cast<float>(tSegment.mTo.mLat),
                              static_cast<float>(tSegment.mTo.mLon)});
            continue;
        }
        const double tSouth = std::fmin(tSegment.mFrom.mLat, tSegment.mTo.mLat);
        const double tNorth = std::fmax(tSegment.mFrom.mLat, tSegment.mTo.mLat);
        int tCells = 0;
        for (double tLat = std::floor(tSouth); tLat <= tNorth && tCells < kMaxCellsPerSegment; tLat += 1.0)
        {
            for (double tLon = std::floor(tWest); tLon <= tEast && tCells < kMaxCellsPerSegment; tLon += 1.0)
            {
                tItems.push_back({static_cast<uint32_t>(tIndex), static_cast<float>(tLat + 0.5),
                                  static_cast<float>(tLon + 0.5)});
                ++tCells;
            }
        }
    }
    mGrid.Build(tItems);
}

void AirwayDatabase::StartLoading(const std::vector<std::string>& pPaths, const NavDatabase& pNav)
{
    if (mJob.IsRunning() || mReady)
    {
        return;
    }
    mJob.Start(
        [this, pPaths, &pNav]()
        {
            // ARKA PLAN THREAD'i: XPLM yok; sonuc mSegments/mGrid/mReport'a yazilir
            for (const std::string& tPath : pPaths)
            {
                if (LoadFile(tPath, pNav, mJob.CancelFlag(), &mReport))
                {
                    return;
                }
            }
            mReport = "earth_awy.dat not found";
        });
}

bool AirwayDatabase::Poll()
{
    if (!mJob.Poll())
    {
        return false;
    }
    mReady = !mSegments.empty();
    return true;
}

bool AirwayDatabase::IsLoading() const
{
    return mJob.IsRunning();
}

bool AirwayDatabase::IsReady() const
{
    return mReady;
}

const std::string& AirwayDatabase::LastReport() const
{
    return mReport;
}

int AirwayDatabase::Count() const
{
    return static_cast<int>(mSegments.size());
}

const AirwaySegment& AirwayDatabase::At(int pIndex) const
{
    return mSegments[pIndex];
}

void AirwayDatabase::Query(const GeoBounds& pBounds, std::vector<int>& pOut) const
{
    if (mReady)
    {
        mGrid.Query(pBounds, pOut);
    }
}
