// Etiket seyreltme: yerlesen kutulari hatirlar, cakisan yeni kutuyu reddeder.
// X-Plane'e bagimli DEGIL.
#pragma once

#include <vector>

#include "ScreenMath.h"

class LabelPlacer
{
public:
    // pArea disina tasan kutu reddedilir
    void reset(const RectD& pArea);

    bool tryPlace(const RectD& pBox);

    // Kontrolsuz ayirma (sembollerin yeri): etiketler bunlarin ustune binmesin
    void reserve(const RectD& pBox);

    int placedCount() const;

private:
    void insert(const RectD& pBox);  // kontrolsuz ekleme (kutu alanin icinde olmali)
    void cellRange(const RectD& pBox, int* pColumnFirst, int* pColumnLast, int* pRowFirst, int* pRowLast) const;

    RectD mArea;
    int mColumns = 0;
    int mRows = 0;
    std::vector<std::vector<int>> mCells;  // hucre -> mBoxes indeksleri
    std::vector<RectD> mBoxes;
};
