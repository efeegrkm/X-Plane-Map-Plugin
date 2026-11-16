// Ekran koordinatlari icin kucuk matematik yardimcilari. X-Plane'e bagimli DEGIL.
// Koordinat sistemleri: lat/lon (derece), boxel (XPLM pencere/fare birimi, UI olcekli),
// piksel (openGL cizim birimi). boxel -> piksel afin donusumudur (Affine2D).
#pragma once

// Ekranda bir nokta (boxel). Cizim icin float hassasiyeti yeterli.
struct PointF
{
    float mX = 0.0f;
    float mY = 0.0f;
};

struct Affine2D
{
    // (x, y) -> (mA*x + mB*y + mTx,  mC*x + mD*y + mTy)
    double mA = 1.0;
    double mB = 0.0;
    double mC = 0.0;
    double mD = 1.0;
    double mTx = 0.0;
    double mTy = 0.0;

    void apply(double pX, double pY, double* pOutX, double* pOutY) const
    {
        *pOutX = mA * pX + mB * pY + mTx;
        *pOutY = mC * pX + mD * pY + mTy;
    }
};

// dikdortgen(sol, ust, sag, alt). y yukari dogru artar: ust > alt.
struct RectD
{
    double mLeft = 0.0;
    double mTop = 0.0;
    double mRight = 0.0;
    double mBottom = 0.0;

    double width() const
    {
        return mRight - mLeft;
    }

    double height() const
    {
        return mTop - mBottom;
    }

    bool contains(double pX, double pY) const
    {
        return pX >= mLeft && pX <= mRight && pY >= mBottom && pY <= mTop;
    }
};

struct RectI
{
    int mLeft = 0;
    int mTop = 0;
    int mRight = 0;
    int mBottom = 0;

    int width() const
    {
        return mRight - mLeft;
    }

    int height() const
    {
        return mTop - mBottom;
    }

    bool contains(int pX, int pY) const
    {
        return pX >= mLeft && pX <= mRight && pY >= mBottom && pY <= mTop;
    }

    bool operator==(const RectI& pOther) const
    {
        return mLeft == pOther.mLeft && mTop == pOther.mTop && mRight == pOther.mRight &&
               mBottom == pOther.mBottom;
    }

    bool operator!=(const RectI& pOther) const
    {
        return !(*this == pOther);
    }
};

// openGL modelview + projection matrislerinden "cizim koordinati -> pencere
// pikseli" afin donusumu. Matrisler sutun oncelikli; perspektif gelirse false.
bool affineFromGL(const float pModelview[16], const float pProjection[16], const int pViewport[4],
                  Affine2D* pOut);

// pSecond(pFirst(p)) donusumunu tek donusume indirger
Affine2D compose(const Affine2D& pSecond, const Affine2D& pFirst);

// Ters donusum. Determinant ~0 ise false.
bool invert(const Affine2D& pMatrix, Affine2D* pOut);

// Dikdortgenin 4 kosesini donusturup onlari kapsayan dikdortgeni verir
RectD transformRectBBox(const Affine2D& pMatrix, const RectD& pRect);

// Iki dikdortgenin kesisimi; kesismiyorsa false
bool intersectRect(const RectD& pFirst, const RectD& pSecond, RectD* pOut);

RectD toRectD(const RectI& pRect);
