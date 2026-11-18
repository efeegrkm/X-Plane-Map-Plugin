#include "GeoGrid.h"

#include <cmath>

namespace
{
const int kColumns = 360;
const int kRows = 180;
const int kCellCount = kColumns * kRows;

int clampInt(int pValue, int pLow, int pHigh)
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

int wrapColumn(int pColumn)
{
    const int tWrapped = pColumn % kColumns;
    return tWrapped < 0 ? tWrapped + kColumns : tWrapped;
}
}

void GeoGrid::clear()
{
    mCellStart.clear();
    mItems.clear();
}

void GeoGrid::build(const std::vector<GridItem>& pItems)
{
    mCellStart.assign(kCellCount + 1, 0);
    for (const GridItem& tItem : pItems)
    {
        ++mCellStart[cellIndex(tItem.mLat, tItem.mLon) + 1];
    }

    for (int tCell = 0; tCell < kCellCount; ++tCell)
    {
        mCellStart[tCell + 1] += mCellStart[tCell];
    }

    mItems.assign(pItems.size(), 0);
    std::vector<uint32_t> tCursor(mCellStart.begin(), mCellStart.end() - 1);
    for (const GridItem& tItem : pItems)
    {
        mItems[tCursor[cellIndex(tItem.mLat, tItem.mLon)]++] = tItem.mIndex;
    }
}

int GeoGrid::count() const
{
    return static_cast<int>(mItems.size());
}

void GeoGrid::query(const GeoBounds& pBounds, std::vector<int>& pOut) const
{
    if (mCellStart.empty())
    {
        return;
    }
    const int tRowFirst = clampInt(static_cast<int>(std::floor(pBounds.mSouth + 90.0)), 0, kRows - 1);
    const int tRowLast = clampInt(static_cast<int>(std::floor(pBounds.mNorth + 90.0)), 0, kRows - 1);
    const int tColumnFirst = static_cast<int>(std::floor(pBounds.mWest + 180.0));
    int tColumnLast = static_cast<int>(std::floor(pBounds.mEast + 180.0));
    if (tColumnLast - tColumnFirst >= kColumns)
    {
        tColumnLast = tColumnFirst + kColumns - 1;
    }

    for (int tRow = tRowFirst; tRow <= tRowLast; ++tRow)
    {
        for (int tColumn = tColumnFirst; tColumn <= tColumnLast; ++tColumn)
        {
            const int tCell = tRow * kColumns + wrapColumn(tColumn);
            for (uint32_t tItem = mCellStart[tCell]; tItem < mCellStart[tCell + 1]; ++tItem)
            {
                pOut.push_back(static_cast<int>(mItems[tItem]));
            }
        }
    }
}

int GeoGrid::cellIndex(float pLat, float pLon)
{
    const int tRow = clampInt(static_cast<int>(std::floor(pLat + 90.0f)), 0, kRows - 1);
    const int tColumn = wrapColumn(static_cast<int>(std::floor(pLon + 180.0f)));
    return tRow * kColumns + tColumn;
}
