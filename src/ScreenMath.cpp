// ScreenMath.cpp
#include "ScreenMath.h"

#include <algorithm>
#include <cmath>

namespace
{

// 4x4 sutun oncelikli carpim: pOut = pFirst * pSecond
void MatMul4(const float* pFirst, const float* pSecond, double* pOut)
{
    for (int tCol = 0; tCol < 4; ++tCol)
    {
        for (int tRow = 0; tRow < 4; ++tRow)
        {
            double tSum = 0.0;
            for (int tK = 0; tK < 4; ++tK)
            {
                tSum += double(pFirst[tK * 4 + tRow]) * double(pSecond[tCol * 4 + tK]);
            }
            pOut[tCol * 4 + tRow] = tSum;
        }
    }
}

}  // namespace

bool AffineFromGL(const float pModelview[16], const float pProjection[16], const int pViewport[4],
                  Affine2D* pOut)
{
    // M = Projection * ModelView : cizim koordinati -> "clip" koordinati
    double tM[16];
    MatMul4(pProjection, pModelview, tM);

    // 2B (ortografik) cizimde w her zaman sabittir: m[3] = m[7] = 0.
    if (std::fabs(tM[3]) > 1e-9 || std::fabs(tM[7]) > 1e-9 || std::fabs(tM[15]) < 1e-12)
    {
        return false;
    }
    const double tW = tM[15];

    // clip -> NDC (-1..1):   ndc = clip / w
    // NDC -> piksel:         px = vx + (ndc + 1) / 2 * vw
    const double tHalfW = pViewport[2] * 0.5;
    const double tHalfH = pViewport[3] * 0.5;

    Affine2D tResult;
    tResult.mA = tM[0] / tW * tHalfW;
    tResult.mB = tM[4] / tW * tHalfW;
    tResult.mTx = pViewport[0] + (tM[12] / tW + 1.0) * tHalfW;
    tResult.mC = tM[1] / tW * tHalfH;
    tResult.mD = tM[5] / tW * tHalfH;
    tResult.mTy = pViewport[1] + (tM[13] / tW + 1.0) * tHalfH;
    *pOut = tResult;
    return true;
}

Affine2D Compose(const Affine2D& pSecond, const Affine2D& pFirst)
{
    // s(f(p)) = S_lin * (F_lin * p + F_t) + S_t
    Affine2D tResult;
    tResult.mA = pSecond.mA * pFirst.mA + pSecond.mB * pFirst.mC;
    tResult.mB = pSecond.mA * pFirst.mB + pSecond.mB * pFirst.mD;
    tResult.mC = pSecond.mC * pFirst.mA + pSecond.mD * pFirst.mC;
    tResult.mD = pSecond.mC * pFirst.mB + pSecond.mD * pFirst.mD;
    tResult.mTx = pSecond.mA * pFirst.mTx + pSecond.mB * pFirst.mTy + pSecond.mTx;
    tResult.mTy = pSecond.mC * pFirst.mTx + pSecond.mD * pFirst.mTy + pSecond.mTy;
    return tResult;
}

bool Invert(const Affine2D& pMatrix, Affine2D* pOut)
{
    const double tDet = pMatrix.mA * pMatrix.mD - pMatrix.mB * pMatrix.mC;
    if (std::fabs(tDet) < 1e-18)
    {
        return false;
    }
    Affine2D tResult;
    tResult.mA = pMatrix.mD / tDet;
    tResult.mB = -pMatrix.mB / tDet;
    tResult.mC = -pMatrix.mC / tDet;
    tResult.mD = pMatrix.mA / tDet;
    tResult.mTx = -(tResult.mA * pMatrix.mTx + tResult.mB * pMatrix.mTy);
    tResult.mTy = -(tResult.mC * pMatrix.mTx + tResult.mD * pMatrix.mTy);
    *pOut = tResult;
    return true;
}

RectD TransformRectBBox(const Affine2D& pMatrix, const RectD& pRect)
{
    double tXs[4];
    double tYs[4];
    pMatrix.Apply(pRect.mLeft, pRect.mTop, &tXs[0], &tYs[0]);
    pMatrix.Apply(pRect.mRight, pRect.mTop, &tXs[1], &tYs[1]);
    pMatrix.Apply(pRect.mRight, pRect.mBottom, &tXs[2], &tYs[2]);
    pMatrix.Apply(pRect.mLeft, pRect.mBottom, &tXs[3], &tYs[3]);
    RectD tResult;
    tResult.mLeft = *std::min_element(tXs, tXs + 4);
    tResult.mRight = *std::max_element(tXs, tXs + 4);
    tResult.mBottom = *std::min_element(tYs, tYs + 4);
    tResult.mTop = *std::max_element(tYs, tYs + 4);
    return tResult;
}

bool IntersectRect(const RectD& pFirst, const RectD& pSecond, RectD* pOut)
{
    RectD tResult;
    tResult.mLeft = std::max(pFirst.mLeft, pSecond.mLeft);
    tResult.mRight = std::min(pFirst.mRight, pSecond.mRight);
    tResult.mBottom = std::max(pFirst.mBottom, pSecond.mBottom);
    tResult.mTop = std::min(pFirst.mTop, pSecond.mTop);
    if (tResult.mRight <= tResult.mLeft || tResult.mTop <= tResult.mBottom)
    {
        return false;
    }
    *pOut = tResult;
    return true;
}

RectD ToRectD(const RectI& pRect)
{
    RectD tResult;
    tResult.mLeft = pRect.mLeft;
    tResult.mTop = pRect.mTop;
    tResult.mRight = pRect.mRight;
    tResult.mBottom = pRect.mBottom;
    return tResult;
}
