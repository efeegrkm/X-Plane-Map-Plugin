// GeoGrid.cpp
#include "GeoGrid.h"

#include <cmath>

namespace
{

const int kColumns = 360;  // 1 derecelik boylam hucreleri
const int kRows = 180;     // 1 derecelik enlem hucreleri
const int kCellCount = kColumns * kRows;

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

void GeoGrid::Clear()
{
    mCellStart.clear();
    mItems.clear();
}

void GeoGrid::Build(const std::vector<GridItem>& pItems)
{
    // 1) Her hucrede kac kayit var
    mCellStart.assign(kCellCount + 1, 0);
    for (const GridItem& tItem : pItems)
    {
        ++mCellStart[CellIndex(tItem.mLat, tItem.mLon) + 1];
    }

    // 2) Birikimli toplam: hucre c'nin ilk kaydinin yeri
    for (int tCell = 0; tCell < kCellCount; ++tCell)
    {
        mCellStart[tCell + 1] += mCellStart[tCell];
    }

    // 3) Kayitlari yerlerine yaz
    mItems.assign(pItems.size(), 0);
    std::vector<uint32_t> tCursor(mCellStart.begin(), mCellStart.end() - 1);
    for (const GridItem& tItem : pItems)
    {
        mItems[tCursor[CellIndex(tItem.mLat, tItem.mLon)]++] = tItem.mIndex;
    }
}

int GeoGrid::Count() const
{
    return static_cast<int>(mItems.size());
}

void GeoGrid::Query(const GeoBounds& pBounds, std::vector<int>& pOut) const
{
    if (mCellStart.empty())
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
            for (uint32_t tItem = mCellStart[tCell]; tItem < mCellStart[tCell + 1]; ++tItem)
            {
                pOut.push_back(static_cast<int>(mItems[tItem]));
            }
        }
    }
}

int GeoGrid::CellIndex(float pLat, float pLon)
{
    const int tRow = ClampInt(static_cast<int>(std::floor(pLat + 90.0f)), 0, kRows - 1);
    const int tColumn = WrapColumn(static_cast<int>(std::floor(pLon + 180.0f)));
    return tRow * kColumns + tColumn;
}
