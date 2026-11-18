#include "data/NavDatabase.h"

#include <cctype>
#include <cmath>
#include <unordered_map>

#include "Geo.h"

namespace
{
const double kVorDmeMaxNm = 1.0;
const double kIlsDmeMaxNm = 3.0;
const int kLowestVhfNavFrequency = 10800;
}

void decodeLocalizerCourse(double pRaw, float* pOutTrue, short* pOutMagnetic)
{
    if (pRaw >= 360.0)
    {
        const double tMagnetic = std::floor(pRaw / 360.0);
        *pOutTrue = static_cast<float>(pRaw - tMagnetic * 360.0);
        *pOutMagnetic = static_cast<short>(tMagnetic);
    }
    else
    {
        *pOutTrue = static_cast<float>(pRaw);
        *pOutMagnetic = -1;
    }
}

bool isMajorAirportId(const std::string& pId)
{
    if (pId.size() != 4)
    {
        return false;
    }
    for (char tChar : pId)
    {
        if (!std::isalpha(static_cast<unsigned char>(tChar)))
        {
            return false;
        }
    }
    return true;
}

AirportKind airportKindFromName(const std::string& pName, std::string* pOutCleanName)
{
    AirportKind tKind = AirportKind::Land;
    if (pName.size() >= 3 && pName[0] == '[' && pName[2] == ']')
    {
        switch (pName[1])
        {
            case 'H':
                tKind = AirportKind::Heliport;
                break;
            case 'S':
                tKind = AirportKind::Seaplane;
                break;
            case 'X':
                tKind = AirportKind::Closed;
                break;
            default:
                *pOutCleanName = pName;
                return AirportKind::Land;
        }
        const size_t tStart = pName.find_first_not_of(' ', 3);
        *pOutCleanName = (tStart == std::string::npos) ? std::string() : pName.substr(tStart);
        return tKind;
    }
    *pOutCleanName = pName;
    return tKind;
}

bool isPlaceholderId(const std::string& pId)
{
    return pId.find_first_not_of('-') == std::string::npos;
}

bool isTacanChannel(int pFrequency)
{
    return pFrequency > 0 && pFrequency < kLowestVhfNavFrequency;
}

void NavDatabase::clear()
{
    mRecords.clear();
    for (GeoGrid& tGrid : mGrids)
    {
        tGrid.clear();
    }
    mReady = false;
}

void NavDatabase::add(const NavRecord& pRecord)
{
    mRecords.push_back(pRecord);
    mReady = false;
}

void NavDatabase::build()
{
    pairDmes();
    buildIndex();
    mReady = true;
}

bool NavDatabase::isReady() const
{
    return mReady;
}

int NavDatabase::count() const
{
    return static_cast<int>(mRecords.size());
}

int NavDatabase::countOf(NavKind pKind) const
{
    return mGrids[static_cast<int>(pKind)].count();
}

const NavRecord& NavDatabase::at(int pIndex) const
{
    return mRecords[pIndex];
}

void NavDatabase::query(NavKind pKind, const GeoBounds& pBounds, std::vector<int>& pOut) const
{
    if (!mReady)
    {
        return;
    }
    mGrids[static_cast<int>(pKind)].query(pBounds, pOut);
}

void NavDatabase::pairDmes()
{
    std::unordered_multimap<std::string, int> tStations;
    for (int tIndex = 0; tIndex < count(); ++tIndex)
    {
        const NavKind tKind = mRecords[tIndex].mKind;
        if (tKind == NavKind::Vor || tKind == NavKind::Ils || tKind == NavKind::Localizer)
        {
            tStations.emplace(mRecords[tIndex].mId, tIndex);
        }
    }

    for (NavRecord& tDme : mRecords)
    {
        if (tDme.mKind != NavKind::Dme)
        {
            continue;
        }
        const LatLon tDmePosition = {tDme.mLat, tDme.mLon};
        const auto tRange = tStations.equal_range(tDme.mId);
        for (auto tCandidate = tRange.first; tCandidate != tRange.second; ++tCandidate)
        {
            NavRecord& tStation = mRecords[tCandidate->second];
            const double tLimit = (tStation.mKind == NavKind::Vor) ? kVorDmeMaxNm : kIlsDmeMaxNm;
            if (distanceNm(tDmePosition, {tStation.mLat, tStation.mLon}) <= tLimit)
            {
                tStation.mHasDme = true;
                tDme.mHidden = true;
                break;
            }
        }
    }
}

void NavDatabase::buildIndex()
{
    std::vector<GridItem> tItems[kNavKindCount];
    for (int tRecordIndex = 0; tRecordIndex < count(); ++tRecordIndex)
    {
        const NavRecord& tRecord = mRecords[tRecordIndex];
        if (!tRecord.mHidden)
        {
            tItems[static_cast<int>(tRecord.mKind)].push_back(
                {static_cast<uint32_t>(tRecordIndex), tRecord.mLat, tRecord.mLon});
        }
    }
    for (int tKind = 0; tKind < kNavKindCount; ++tKind)
    {
        mGrids[tKind].build(tItems[tKind]);
    }
}
