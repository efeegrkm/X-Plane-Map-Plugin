// ScreenMath.h
// Ekran koordinatlari icin kucuk matematik yardimcilari. X-Plane'e bagimli DEGIL.
//
// Bu projede 3 koordinat sistemi var:
//   1. lat/lon : cografi konum (derece)
//   2. boxel   : XPLM pencerelerinin ve farenin birimi (global masaustu).
//                UI olcekleme %100 degilse pikselden farklidir.
//   3. piksel  : OpenGL'in cizim yaptigi gercek ekran pikselleri
//
// lat/lon <-> boxel donusumunu kendi projeksiyonumuz (MapView) yapar.
// boxel -> piksel donusumunu ise pencere cizilirken OpenGL'in o anki
// matrislerinden okuruz (sadece glScissor gibi piksel isteyen cagrilar icin).
// Bu donusum "afin"dir (olcek + kaydirma), Affine2D ile ifade edilir.
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

    void Apply(double pX, double pY, double* pOutX, double* pOutY) const
    {
        *pOutX = mA * pX + mB * pY + mTx;
        *pOutY = mC * pX + mD * pY + mTy;
    }
};

// Dikdortgen (sol, ust, sag, alt). y yukari dogru artar: ust > alt.
struct RectD
{
    double mLeft = 0.0;
    double mTop = 0.0;
    double mRight = 0.0;
    double mBottom = 0.0;

    double Width() const
    {
        return mRight - mLeft;
    }

    double Height() const
    {
        return mTop - mBottom;
    }

    bool Contains(double pX, double pY) const
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

    int Width() const
    {
        return mRight - mLeft;
    }

    int Height() const
    {
        return mTop - mBottom;
    }

    bool Contains(int pX, int pY) const
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

// OpenGL modelview + projection matrisleri ve viewport'tan
// "cizim koordinati -> pencere pikseli" afin donusumunu cikarir.
// Matrisler OpenGL sirasinda (sutun oncelikli) 16 float'tir.
// Perspektif (2B olmayan) bir matris gelirse false doner.
bool AffineFromGL(const float pModelview[16], const float pProjection[16], const int pViewport[4],
                  Affine2D* pOut);

// pSecond(pFirst(p)) donusumunu tek donusume indirger
Affine2D Compose(const Affine2D& pSecond, const Affine2D& pFirst);

// Ters donusum. Determinant ~0 ise false.
bool Invert(const Affine2D& pMatrix, Affine2D* pOut);

// Dikdortgenin 4 kosesini donusturup onlari kapsayan dikdortgeni verir
RectD TransformRectBBox(const Affine2D& pMatrix, const RectD& pRect);

// Iki dikdortgenin kesisimi. Kesismiyorsa false.
bool IntersectRect(const RectD& pFirst, const RectD& pSecond, RectD* pOut);

RectD ToRectD(const RectI& pRect);
