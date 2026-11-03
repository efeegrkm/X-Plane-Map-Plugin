// NavDatabase.cpp
#include "data/NavDatabase.h"

#include <cctype>
#include <cmath>
#include <unordered_map>

#include "Geo.h"

namespace
{

// DME'nin eslestigi istasyona en fazla bu kadar uzak olabilir.
// ILS'in DME'si genelde suzulus yolu anteninin yaninda, localizer ise pistin
// obur ucunda: pist boyu kadar (birkac km) fark olur.
const double kVorDmeMaxNm = 1.0;
const double kIlsDmeMaxNm = 3.0;
const int kLowestVhfNavFrequency = 10800;  // 108.00 MHz (nav.dat: MHz * 100)

}  // namespace

void DecodeLocalizerCourse(double pRaw, float* pOutTrue, short* pOutMagnetic)
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

bool IsMajorAirportId(const std::string& pId)
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

bool IsPlaceholderId(const std::string& pId)
{
    return pId.find_first_not_of('-') == std::string::npos;  // bos dizgi de buraya duser
}

bool IsTacanChannel(int pFrequency)
{
    return pFrequency > 0 && pFrequency < kLowestVhfNavFrequency;
}

void NavDatabase::Clear()
{
    mRecords.clear();
    for (GeoGrid& tGrid : mGrids)
    {
        tGrid.Clear();
    }
    mReady = false;
}

void NavDatabase::Add(const NavRecord& pRecord)
{
    mRecords.push_back(pRecord);
    mReady = false;
}

void NavDatabase::Build()
{
    PairDmes();
    BuildIndex();
    mReady = true;
}

bool NavDatabase::IsReady() const
{
    return mReady;
}

int NavDatabase::Count() const
{
    return static_cast<int>(mRecords.size());
}

int NavDatabase::CountOf(NavKind pKind) const
{
    return mGrids[static_cast<int>(pKind)].Count();
}

const NavRecord& NavDatabase::At(int pIndex) const
{
    return mRecords[pIndex];
}

void NavDatabase::Query(NavKind pKind, const GeoBounds& pBounds, std::vector<int>& pOut) const
{
    if (!mReady)
    {
        return;
    }
    mGrids[static_cast<int>(pKind)].Query(pBounds, pOut);
}

void NavDatabase::PairDmes()
{
    // Kimlik -> o kimlikteki VOR/ILS/LOC kayitlari
    std::unordered_multimap<std::string, int> tStations;
    for (int tIndex = 0; tIndex < Count(); ++tIndex)
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
            if (DistanceNm(tDmePosition, {tStation.mLat, tStation.mLon}) <= tLimit)
            {
                tStation.mHasDme = true;
                tDme.mHidden = true;
                break;
            }
        }
    }
}

void NavDatabase::BuildIndex()
{
    // Kayitlari turlerine gore ayir (gizli DME'ler izgaraya girmez)
    std::vector<GridItem> tItems[kNavKindCount];
    for (int tRecordIndex = 0; tRecordIndex < Count(); ++tRecordIndex)
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
        mGrids[tKind].Build(tItems[tKind]);
    }
}
