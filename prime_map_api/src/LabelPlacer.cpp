// LabelPlacer.cpp
#include "LabelPlacer.h"

#include <cmath>

namespace
{

const double kCellSize = 32.0;  // boxel

bool Overlaps(const RectD& pFirst, const RectD& pSecond)
{
    return pFirst.mLeft < pSecond.mRight && pSecond.mLeft < pFirst.mRight && pFirst.mBottom < pSecond.mTop &&
           pSecond.mBottom < pFirst.mTop;
}

}  // namespace

void LabelPlacer::Reset(const RectD& pArea)
{
    mArea = pArea;
    mColumns = static_cast<int>(std::ceil(pArea.Width() / kCellSize));
    mRows = static_cast<int>(std::ceil(pArea.Height() / kCellSize));
    if (mColumns < 1)
    {
        mColumns = 1;
    }
    if (mRows < 1)
    {
        mRows = 1;
    }
    const size_t tCellCount = static_cast<size_t>(mColumns) * static_cast<size_t>(mRows);
    if (mCells.size() < tCellCount)
    {
        mCells.resize(tCellCount);
    }
    for (std::vector<int>& tCell : mCells)
    {
        tCell.clear();  // kapasite korunur: her karede yeniden bellek ayrilmaz
    }
    mBoxes.clear();
}

bool LabelPlacer::TryPlace(const RectD& pBox)
{
    if (pBox.mLeft < mArea.mLeft || pBox.mRight > mArea.mRight || pBox.mBottom < mArea.mBottom ||
        pBox.mTop > mArea.mTop)
    {
        return false;
    }

    int tColumnFirst = 0;
    int tColumnLast = 0;
    int tRowFirst = 0;
    int tRowLast = 0;
    CellRange(pBox, &tColumnFirst, &tColumnLast, &tRowFirst, &tRowLast);

    for (int tRow = tRowFirst; tRow <= tRowLast; ++tRow)
    {
        for (int tColumn = tColumnFirst; tColumn <= tColumnLast; ++tColumn)
        {
            for (int tBoxIndex : mCells[tRow * mColumns + tColumn])
            {
                if (Overlaps(mBoxes[tBoxIndex], pBox))
                {
                    return false;
                }
            }
        }
    }
    Insert(pBox);
    return true;
}

void LabelPlacer::Reserve(const RectD& pBox)
{
    RectD tClipped;
    if (IntersectRect(pBox, mArea, &tClipped))
    {
        Insert(tClipped);
    }
}

void LabelPlacer::Insert(const RectD& pBox)
{
    int tColumnFirst = 0;
    int tColumnLast = 0;
    int tRowFirst = 0;
    int tRowLast = 0;
    CellRange(pBox, &tColumnFirst, &tColumnLast, &tRowFirst, &tRowLast);

    const int tNewIndex = static_cast<int>(mBoxes.size());
    mBoxes.push_back(pBox);
    for (int tRow = tRowFirst; tRow <= tRowLast; ++tRow)
    {
        for (int tColumn = tColumnFirst; tColumn <= tColumnLast; ++tColumn)
        {
            mCells[tRow * mColumns + tColumn].push_back(tNewIndex);
        }
    }
}

int LabelPlacer::PlacedCount() const
{
    return static_cast<int>(mBoxes.size());
}

void LabelPlacer::CellRange(const RectD& pBox, int* pColumnFirst, int* pColumnLast, int* pRowFirst,
                            int* pRowLast) const
{
    *pColumnFirst = static_cast<int>((pBox.mLeft - mArea.mLeft) / kCellSize);
    *pColumnLast = static_cast<int>((pBox.mRight - mArea.mLeft) / kCellSize);
    *pRowFirst = static_cast<int>((pBox.mBottom - mArea.mBottom) / kCellSize);
    *pRowLast = static_cast<int>((pBox.mTop - mArea.mBottom) / kCellSize);
    if (*pColumnLast >= mColumns)
    {
        *pColumnLast = mColumns - 1;
    }
    if (*pRowLast >= mRows)
    {
        *pRowLast = mRows - 1;
    }
}
