// ScreenMath.h
// Koordinat sistemleri arasi donusum matematigi. X-Plane'e bagimli DEGIL.
//
// Bu projede 4 koordinat sistemi var:
//   1. lat/lon        : cografi konum (derece)
//   2. harita birimi  : X-Plane haritasinin kendi koordinati
//                       (lat/lon <-> harita birimi: XPLMMapProject/Unproject)
//   3. piksel         : OpenGL'in cizim yaptigi ekran pikselleri
//   4. boxel          : XPLM pencerelerinin ve farenin birimi
//                       (UI olcekleme %100 degilse pikselden farklidir)
//
// Harita birimi -> piksel ve boxel -> piksel donusumlerini OpenGL'in o anki
// matrislerinden okuyoruz. Ikisi de "afin" donusumdur (olcek + donme +
// kaydirma), bu yuzden 2x2 matris + kaydirma (Affine2D) ile ifade edilir,
// tersleri alinabilir ve birbirine baglanabilir:
//
//   tiklama (boxel) -> piksel -> harita birimi -> lat/lon
#pragma once

struct Affine2D {
    // (x, y) -> (a*x + b*y + tx,  c*x + d*y + ty)
    double a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;

    void Apply(double x, double y, double* outX, double* outY) const {
        *outX = a * x + b * y + tx;
        *outY = c * x + d * y + ty;
    }
};

// Dikdortgen (sol, ust, sag, alt). y yukari dogru artar: top > bottom.
struct RectD {
    double left = 0, top = 0, right = 0, bottom = 0;
    double Width() const { return right - left; }
    double Height() const { return top - bottom; }
};

struct RectI {
    int left = 0, top = 0, right = 0, bottom = 0;
    int Width() const { return right - left; }
    int Height() const { return top - bottom; }
    bool operator==(const RectI& o) const {
        return left == o.left && top == o.top && right == o.right && bottom == o.bottom;
    }
    bool operator!=(const RectI& o) const { return !(*this == o); }
};

// OpenGL modelview + projection matrisleri ve viewport'tan
// "cizim koordinati -> pencere pikseli" afin donusumunu cikarir.
// Matrisler OpenGL sirasinda (sutun oncelikli) 16 float'tir.
// Perspektif (2B olmayan) bir matris gelirse false doner.
bool AffineFromGL(const float modelview[16], const float projection[16],
                  const int viewport[4], Affine2D* out);

// second(first(p)) donusumunu tek donusume indirger
Affine2D Compose(const Affine2D& second, const Affine2D& first);

// Ters donusum. Determinant ~0 ise false.
bool Invert(const Affine2D& m, Affine2D* out);

// Dikdortgenin 4 kosesini donusturup onlari kapsayan dikdortgeni verir
RectD TransformRectBBox(const Affine2D& m, const RectD& r);

// Iki dikdortgenin kesisimi. Kesismiyorsa false.
bool IntersectRect(const RectD& a, const RectD& b, RectD* out);

// En yakin tamsayiya yuvarlanmis dikdortgen
RectI RoundRect(const RectD& r);

// Donusumde donme (rotation) var mi? (harita "track up" donuk ise olur)
bool HasRotation(const Affine2D& m);
