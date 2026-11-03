// ScreenMath.cpp
#include "ScreenMath.h"

#include <algorithm>
#include <cmath>

// 4x4 sutun oncelikli carpim: out = a * b
static void MatMul4(const float* a, const float* b, double* out) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            double s = 0;
            for (int k = 0; k < 4; ++k) s += double(a[k * 4 + row]) * double(b[col * 4 + k]);
            out[col * 4 + row] = s;
        }
    }
}

bool AffineFromGL(const float modelview[16], const float projection[16],
                  const int viewport[4], Affine2D* out) {
    // M = Projection * ModelView : cizim koordinati -> "clip" koordinati
    double m[16];
    MatMul4(projection, modelview, m);

    // 2B (ortografik) cizimde w her zaman sabittir: m[3] = m[7] = 0.
    // (z = 0 cizdigimiz icin m[11] onemsiz.)
    if (std::fabs(m[3]) > 1e-9 || std::fabs(m[7]) > 1e-9 || std::fabs(m[15]) < 1e-12) {
        return false;
    }
    const double w = m[15];

    // clip -> NDC (-1..1):   ndc = clip / w
    // NDC -> piksel:         px = vx + (ndc + 1) / 2 * vw
    const double hx = viewport[2] * 0.5;
    const double hy = viewport[3] * 0.5;

    Affine2D r;
    r.a = m[0] / w * hx;
    r.b = m[4] / w * hx;
    r.tx = viewport[0] + (m[12] / w + 1.0) * hx;
    r.c = m[1] / w * hy;
    r.d = m[5] / w * hy;
    r.ty = viewport[1] + (m[13] / w + 1.0) * hy;
    *out = r;
    return true;
}

Affine2D Compose(const Affine2D& s, const Affine2D& f) {
    // s(f(p)) = S_lin * (F_lin * p + F_t) + S_t
    Affine2D r;
    r.a = s.a * f.a + s.b * f.c;
    r.b = s.a * f.b + s.b * f.d;
    r.c = s.c * f.a + s.d * f.c;
    r.d = s.c * f.b + s.d * f.d;
    r.tx = s.a * f.tx + s.b * f.ty + s.tx;
    r.ty = s.c * f.tx + s.d * f.ty + s.ty;
    return r;
}

bool Invert(const Affine2D& m, Affine2D* out) {
    double det = m.a * m.d - m.b * m.c;
    if (std::fabs(det) < 1e-18) return false;
    Affine2D r;
    r.a = m.d / det;
    r.b = -m.b / det;
    r.c = -m.c / det;
    r.d = m.a / det;
    r.tx = -(r.a * m.tx + r.b * m.ty);
    r.ty = -(r.c * m.tx + r.d * m.ty);
    *out = r;
    return true;
}

RectD TransformRectBBox(const Affine2D& m, const RectD& r) {
    double xs[4], ys[4];
    m.Apply(r.left, r.top, &xs[0], &ys[0]);
    m.Apply(r.right, r.top, &xs[1], &ys[1]);
    m.Apply(r.right, r.bottom, &xs[2], &ys[2]);
    m.Apply(r.left, r.bottom, &xs[3], &ys[3]);
    RectD o;
    o.left = *std::min_element(xs, xs + 4);
    o.right = *std::max_element(xs, xs + 4);
    o.bottom = *std::min_element(ys, ys + 4);
    o.top = *std::max_element(ys, ys + 4);
    return o;
}

bool IntersectRect(const RectD& a, const RectD& b, RectD* out) {
    RectD o;
    o.left = std::max(a.left, b.left);
    o.right = std::min(a.right, b.right);
    o.bottom = std::max(a.bottom, b.bottom);
    o.top = std::min(a.top, b.top);
    if (o.right <= o.left || o.top <= o.bottom) return false;
    *out = o;
    return true;
}

RectI RoundRect(const RectD& r) {
    RectI o;
    o.left = static_cast<int>(std::lround(r.left));
    o.top = static_cast<int>(std::lround(r.top));
    o.right = static_cast<int>(std::lround(r.right));
    o.bottom = static_cast<int>(std::lround(r.bottom));
    return o;
}

bool HasRotation(const Affine2D& m) {
    double scale = std::max(std::fabs(m.a), std::fabs(m.d));
    if (scale < 1e-18) return false;
    return std::fabs(m.b) > 1e-6 * scale || std::fabs(m.c) > 1e-6 * scale;
}
