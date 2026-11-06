// TerrainVectors.inl
// FillVectorArea: sablon (piksel ziyaretcisi satir icinde tek cagri; std::function maliyeti yok).
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

template <typename TSet>
void FillVectorArea(const VectorArea& pArea, double pX0, double pYTop, double pPixelsPerUnitX,
                    double pPixelsPerUnitY, int pWidth, int pHeight, TSet pSet)
{
    // Alanin satir araligi (izgaraya kirpilmis)
    const int tFirstRow = std::max(0, static_cast<int>(std::floor((pYTop - pArea.mBounds[3]) * pPixelsPerUnitY)));
    const int tLastRow =
        std::min(pHeight - 1, static_cast<int>(std::ceil((pYTop - pArea.mBounds[1]) * pPixelsPerUnitY)));
    if (tFirstRow > tLastRow)
    {
        return;
    }
    const size_t tRingCount = pArea.mRingStarts.size();
    std::vector<double> tCrossings;
    for (int tRow = tFirstRow; tRow <= tLastRow; ++tRow)
    {
        // Satir merkezinin y'si; her halka kenarinin bu yatayi kestigi x'ler (cift-tek kurali)
        const double tY = pYTop - (tRow + 0.5) / pPixelsPerUnitY;
        tCrossings.clear();
        for (size_t tRing = 0; tRing < tRingCount; ++tRing)
        {
            const size_t tBegin = pArea.mRingStarts[tRing];
            const size_t tEnd = (tRing + 1 < tRingCount) ? pArea.mRingStarts[tRing + 1] : pArea.mX.size();
            if (tEnd - tBegin < 3)
            {
                continue;
            }
            size_t tPrevious = tEnd - 1;
            for (size_t tIndex = tBegin; tIndex < tEnd; tPrevious = tIndex++)
            {
                const double tYa = pArea.mY[tPrevious];
                const double tYb = pArea.mY[tIndex];
                if ((tYa > tY) != (tYb > tY))
                {
                    const double tXa = pArea.mX[tPrevious];
                    const double tXb = pArea.mX[tIndex];
                    tCrossings.push_back(tXa + (tY - tYa) / (tYb - tYa) * (tXb - tXa));
                }
            }
        }
        std::sort(tCrossings.begin(), tCrossings.end());
        for (size_t tPair = 0; tPair + 1 < tCrossings.size(); tPair += 2)
        {
            // Merkezi [giris, cikis) araliginda kalan pikseller
            const int tFirst =
                std::max(0, static_cast<int>(std::ceil((tCrossings[tPair] - pX0) * pPixelsPerUnitX - 0.5)));
            const int tLast = std::min(
                pWidth - 1, static_cast<int>(std::ceil((tCrossings[tPair + 1] - pX0) * pPixelsPerUnitX - 0.5)) - 1);
            for (int tColumn = tFirst; tColumn <= tLast; ++tColumn)
            {
                pSet(tColumn, tRow);
            }
        }
    }
}
