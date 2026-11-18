// Etiket ve sembol kutularinin cakismasini onleyen yerlestirici.
#ifndef PRIME_MAP_SRC_LABELPLACER_H
#define PRIME_MAP_SRC_LABELPLACER_H

#include <vector>

#include "ScreenMath.h"

class LabelPlacer
{
public:
    void reset(const RectD& pArea);

    bool tryPlace(const RectD& pBox);

    void reserve(const RectD& pBox);

    int placedCount() const;

private:
    void insert(const RectD& pBox);
    void cellRange(const RectD& pBox, int* pColumnFirst, int* pColumnLast, int* pRowFirst, int* pRowLast) const;

    RectD mArea;
    int mColumns = 0;
    int mRows = 0;
    std::vector<std::vector<int>> mCells;
    std::vector<RectD> mBoxes;
};
#endif
