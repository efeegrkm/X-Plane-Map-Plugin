// NavDatabase.cpp
#include "data/NavDatabase.h"

#include <cctype>
#include <cmath>
#include <unordered_map>

#include "Geo.h"

namespace
{

const int kColumns = 360;               // 1 derecelik boylam hucreleri
const int kRows = 180;                  // 1 derecelik enlem hucreleri
const int kCellCount = kColumns * kRows;

// DME'nin eslestigi istasyona en fazla bu kadar uzak olabilir.
// ILS'in DME'si genelde suzulus yolu anteninin yaninda, localizer ise pistin
// obur ucunda: pist boyu kadar (birkac km) fark olur.
const double kVorDmeMaxNm = 1.0;
const double kIlsDmeMaxNm = 3.0;

int ClampInt(int pValue, int pLow, int pHigh)
{
    if (pValue < pLow)
    {
        return pLow;
    }
    if (pValue > pHigh)
    {
        return pHigh;
    }
    return pValue;
}

int WrapColumn(int pColumn)
{
    const int tWrapped = pColumn % kColumns;
    return tWrapped < 0 ? tWrapped + kColumns : tWrapped;
}

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

void NavDatabase::Clear()
{
    mRecords.clear();
    for (KindIndex& tIndex : mIndex)
    {
        tIndex.mCellStart.clear();
        tIndex.mItems.clear();
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
    return static_cast<int>(mIndex[static_cast<int>(pKind)].mItems.size());
}

const NavRecord& NavDatabase::At(int pIndex) const
{
    return mRecords[pIndex];
}

void NavDatabase::Query(NavKind pKind, const GeoBounds& pBounds, std::vector<int>& pOut) const
{
    const KindIndex& tIndex = mIndex[static_cast<int>(pKind)];
    if (!mReady || tIndex.mCellStart.empty())
    {
        return;
    }
    const int tRowFirst = ClampInt(static_cast<int>(std::floor(pBounds.mSouth + 90.0)), 0, kRows - 1);
    const int tRowLast = ClampInt(static_cast<int>(std::floor(pBounds.mNorth + 90.0)), 0, kRows - 1);
    const int tColumnFirst = static_cast<int>(std::floor(pBounds.mWest + 180.0));
    int tColumnLast = static_cast<int>(std::floor(pBounds.mEast + 180.0));
    if (tColumnLast - tColumnFirst >= kColumns)
    {
        tColumnLast = tColumnFirst + kColumns - 1;  // tum dunya: her sutun bir kez
    }

    for (int tRow = tRowFirst; tRow <= tRowLast; ++tRow)
    {
        for (int tColumn = tColumnFirst; tColumn <= tColumnLast; ++tColumn)
        {
            const int tCell = tRow * kColumns + WrapColumn(tColumn);
            const uint32_t tBegin = tIndex.mCellStart[tCell];
            const uint32_t tEnd = tIndex.mCellStart[tCell + 1];
            for (uint32_t tItem = tBegin; tItem < tEnd; ++tItem)
            {
                pOut.push_back(static_cast<int>(tIndex.mItems[tItem]));
            }
        }
    }
}

int NavDatabase::CellIndex(float pLat, float pLon)
{
    const int tRow = ClampInt(static_cast<int>(std::floor(pLat + 90.0f)), 0, kRows - 1);
    const int tColumn = WrapColumn(static_cast<int>(std::floor(pLon + 180.0f)));
    return tRow * kColumns + tColumn;
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
    for (int tKind = 0; tKind < kNavKindCount; ++tKind)
    {
        KindIndex& tIndex = mIndex[tKind];

        // 1) Her hucrede kac kayit var
        tIndex.mCellStart.assign(kCellCount + 1, 0);
        for (const NavRecord& tRecord : mRecords)
        {
            if (static_cast<int>(tRecord.mKind) == tKind && !tRecord.mHidden)
            {
                ++tIndex.mCellStart[CellIndex(tRecord.mLat, tRecord.mLon) + 1];
            }
        }

        // 2) Birikimli toplam: hucre c'nin ilk kaydinin yeri
        for (int tCell = 0; tCell < kCellCount; ++tCell)
        {
            tIndex.mCellStart[tCell + 1] += tIndex.mCellStart[tCell];
        }

        // 3) Kayitlari yerlerine yaz
        tIndex.mItems.assign(tIndex.mCellStart[kCellCount], 0);
        std::vector<uint32_t> tCursor(tIndex.mCellStart.begin(), tIndex.mCellStart.end() - 1);
        for (int tRecordIndex = 0; tRecordIndex < Count(); ++tRecordIndex)
        {
            const NavRecord& tRecord = mRecords[tRecordIndex];
            if (static_cast<int>(tRecord.mKind) == tKind && !tRecord.mHidden)
            {
                const int tCell = CellIndex(tRecord.mLat, tRecord.mLon);
                tIndex.mItems[tCursor[tCell]++] = static_cast<uint32_t>(tRecordIndex);
            }
        }
    }
}
