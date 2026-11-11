// LandColorField.cpp
#include "data/LandColorField.h"

#include <algorithm>
#include <cmath>

#include "data/TerrainClasses.h"

namespace
{

const int kPasses = 3;              // 3 kutu gecisi ~ Gaussian
const double kMaxReachCells = 8.0;  // izgarada erim bundan buyukse hucre buyutulur (hiz / bellek)
const double kMinReachPixels = 1.5; // erim bundan kucukse yumusatma yok

}  // namespace

LandBlendPlan PlanLandBlend(double pMeters, double pMetersPerPixelX, double pMetersPerPixelY,
                            double pMaxReachPixels)
{
    LandBlendPlan tPlan;
    if (pMeters <= 0.0 || pMetersPerPixelX <= 0.0 || pMetersPerPixelY <= 0.0)
    {
        return tPlan;
    }
    // pMaxReachPixels: erimin doku pikseli cinsinden tavani (0 = tavan yok). Yakin seviyelerde blur'u
    // kenar yumusatma duzeyinde tutar; uzak seviyelerde metre degeri zaten bu tavanin altindadir.
    const double tCap = pMaxReachPixels > 0.0 ? pMaxReachPixels : 1e9;
    const double tReachX = std::min(pMeters / pMetersPerPixelX, tCap);  // piksel
    const double tReachY = std::min(pMeters / pMetersPerPixelY, tCap);
    const double tReach = std::max(tReachX, tReachY);
    if (tReach < kMinReachPixels)
    {
        return tPlan;
    }
    tPlan.mFactor = std::max(1, static_cast<int>(std::ceil(tReach / kMaxReachCells)));
    // 3 gecis x yari genislik h -> toplam erim ~3h hucre
    tPlan.mHalfX = std::max(1, static_cast<int>(std::lround(tReachX / tPlan.mFactor / kPasses)));
    tPlan.mHalfY = std::max(1, static_cast<int>(std::lround(tReachY / tPlan.mFactor / kPasses)));
    // Pay: erim + cift dogrusal okuma icin bir hucre
    tPlan.mPad = static_cast<int>(std::ceil(tReach)) + tPlan.mFactor;
    return tPlan;
}

void LandColorField::Reset(int pPixels, int pFactor)
{
    mFactor = std::max(1, pFactor);
    mCells = (pPixels + mFactor - 1) / mFactor;
    mData.assign(static_cast<size_t>(mCells) * mCells * 4, 0.0f);
}

void LandColorField::Add(int pPixelX, int pPixelY, const TerrainColor& pColor, float pWeight)
{
    const int tX = pPixelX / mFactor;
    const int tY = pPixelY / mFactor;
    if (tX < 0 || tY < 0 || tX >= mCells || tY >= mCells || pWeight <= 0.0f)
    {
        return;
    }
    float* tCell = mData.data() + (static_cast<size_t>(tY) * mCells + tX) * 4;
    tCell[0] += pColor.mR * pWeight;
    tCell[1] += pColor.mG * pWeight;
    tCell[2] += pColor.mB * pWeight;
    tCell[3] += pWeight;
}

void LandColorField::BoxPass(std::vector<float>& pData, int pCells, int pHalf, bool pHorizontal)
{
    // Kayan toplam: her hucre [i - h, i + h] penceresinin ortalamasi. Pencere disi 0 (veri yok);
    // renk ve agirlik ayni sekilde etkilendigi icin bolumde dogru ortalama cikar.
    const double tScale = 1.0 / (2 * pHalf + 1);
    std::vector<double> tLine(static_cast<size_t>(pCells) * 4);
    for (int tLineIndex = 0; tLineIndex < pCells; ++tLineIndex)
    {
        auto tCell = [&](int pIndex) -> float*
        {
            const int tX = pHorizontal ? pIndex : tLineIndex;
            const int tY = pHorizontal ? tLineIndex : pIndex;
            return pData.data() + (static_cast<size_t>(tY) * pCells + tX) * 4;
        };
        for (int tIndex = 0; tIndex < pCells; ++tIndex)
        {
            const float* tSource = tCell(tIndex);
            for (int tChannel = 0; tChannel < 4; ++tChannel)
            {
                tLine[static_cast<size_t>(tIndex) * 4 + tChannel] = tSource[tChannel];
            }
        }
        double tSum[4] = {0.0, 0.0, 0.0, 0.0};
        for (int tIndex = 0; tIndex <= pHalf && tIndex < pCells; ++tIndex)
        {
            for (int tChannel = 0; tChannel < 4; ++tChannel)
            {
                tSum[tChannel] += tLine[static_cast<size_t>(tIndex) * 4 + tChannel];
            }
        }
        for (int tIndex = 0; tIndex < pCells; ++tIndex)
        {
            float* tTarget = tCell(tIndex);
            for (int tChannel = 0; tChannel < 4; ++tChannel)
            {
                tTarget[tChannel] = static_cast<float>(std::max(0.0, tSum[tChannel] * tScale));
            }
            const int tAdd = tIndex + pHalf + 1;
            const int tRemove = tIndex - pHalf;
            for (int tChannel = 0; tChannel < 4; ++tChannel)
            {
                if (tAdd < pCells)
                {
                    tSum[tChannel] += tLine[static_cast<size_t>(tAdd) * 4 + tChannel];
                }
                if (tRemove >= 0)
                {
                    tSum[tChannel] -= tLine[static_cast<size_t>(tRemove) * 4 + tChannel];
                }
            }
        }
    }
}

void LandColorField::Blur(int pHalfX, int pHalfY)
{
    for (int tPass = 0; tPass < kPasses; ++tPass)
    {
        if (pHalfX > 0)
        {
            BoxPass(mData, mCells, pHalfX, true);
        }
        if (pHalfY > 0)
        {
            BoxPass(mData, mCells, pHalfY, false);
        }
    }
}

bool LandColorField::ColorAt(int pPixelX, int pPixelY, double pRgb[3]) const
{
    // Piksel merkezinin hucre koordinati (hucre merkezleri x.5'te)
    const double tX = (pPixelX + 0.5) / mFactor - 0.5;
    const double tY = (pPixelY + 0.5) / mFactor - 0.5;
    const int tX0 = std::clamp(static_cast<int>(std::floor(tX)), 0, mCells - 1);
    const int tY0 = std::clamp(static_cast<int>(std::floor(tY)), 0, mCells - 1);
    const int tX1 = std::min(tX0 + 1, mCells - 1);
    const int tY1 = std::min(tY0 + 1, mCells - 1);
    const double tFx = std::clamp(tX - tX0, 0.0, 1.0);
    const double tFy = std::clamp(tY - tY0, 0.0, 1.0);
    double tSum[4] = {0.0, 0.0, 0.0, 0.0};
    const int tXs[2] = {tX0, tX1};
    const int tYs[2] = {tY0, tY1};
    const double tWx[2] = {1.0 - tFx, tFx};
    const double tWy[2] = {1.0 - tFy, tFy};
    for (int tJ = 0; tJ < 2; ++tJ)
    {
        for (int tI = 0; tI < 2; ++tI)
        {
            const float* tCell = mData.data() + (static_cast<size_t>(tYs[tJ]) * mCells + tXs[tI]) * 4;
            const double tW = tWx[tI] * tWy[tJ];
            for (int tChannel = 0; tChannel < 4; ++tChannel)
            {
                tSum[tChannel] += tCell[tChannel] * tW;
            }
        }
    }
    if (tSum[3] < 1e-6)
    {
        return false;
    }
    for (int tChannel = 0; tChannel < 3; ++tChannel)
    {
        pRgb[tChannel] = tSum[tChannel] / tSum[3];
    }
    return true;
}
