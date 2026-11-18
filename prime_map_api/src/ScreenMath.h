// Ekran koordinatlari icin dikdortgen ve afin donusum yardimcilari.
#ifndef PRIME_MAP_SRC_SCREENMATH_H
#define PRIME_MAP_SRC_SCREENMATH_H

struct PointF
{
    float mX = 0.0f;
    float mY = 0.0f;
};

struct Affine2D
{
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

bool affineFromGL(const float pModelview[16], const float pProjection[16], const int pViewport[4],
                  Affine2D* pOut);

Affine2D compose(const Affine2D& pSecond, const Affine2D& pFirst);

bool invert(const Affine2D& pMatrix, Affine2D* pOut);

RectD transformRectBBox(const Affine2D& pMatrix, const RectD& pRect);

bool intersectRect(const RectD& pFirst, const RectD& pSecond, RectD* pOut);

RectD toRectD(const RectI& pRect);

RectI rectFromTopLeft(int pLeft, int pTop, int pWidth, int pHeight);
#endif
