// LabelPlacer.h
// Etiket (ve sembol) seyreltme: ekrana konan kutulari hatirlar, yeni kutu
// oncekilerden biriyle cakisiyorsa reddeder. X-Plane'e bagimli DEGIL.
//
// Her karede Reset ile bosaltilir. Kim once yer isterse o kazanir; bu yuzden
// katmanlar etiketlerini onem sirasiyla ister (havalimani once, fix en son).
// Hizli olsun diye alan 32 boxel'lik hucrelere bolunur; yeni kutu sadece
// degdigi hucrelerdeki kutularla karsilastirilir.
#pragma once

#include <vector>

#include "ScreenMath.h"

class LabelPlacer
{
public:
    // pArea disina tasan kutu kabul edilmez
    void Reset(const RectD& pArea);

    // Kutu bossa yerini ayirir ve true doner; cakisiyorsa false
    bool TryPlace(const RectD& pBox);

    int PlacedCount() const;

private:
    void CellRange(const RectD& pBox, int* pColumnFirst, int* pColumnLast, int* pRowFirst, int* pRowLast) const;

    RectD mArea;
    int mColumns = 0;
    int mRows = 0;
    std::vector<std::vector<int>> mCells;  // hucre -> mBoxes indeksleri
    std::vector<RectD> mBoxes;
};
