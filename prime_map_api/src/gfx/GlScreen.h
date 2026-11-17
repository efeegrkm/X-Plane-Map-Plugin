// boxel -> piksel donusumu ve scissor kirpma. XPLM boxel, glScissor gercek
// piksel ister; donusum draw callback'te acik GL matrislerinden okunur.
#ifndef PRIME_MAP_SRC_GFX_GLSCREEN_H
#define PRIME_MAP_SRC_GFX_GLSCREEN_H

#include <array>
#include <optional>

#include "ScreenMath.h"

// Sadece bir XPLM penceresinin draw callback'i icinde anlamlidir.
std::optional<Affine2D> readBoxelToPixel();

// boxel basina piksel orani (UI olcek: 1.0, 1.5, 2.0 ...)
double pixelsPerBoxel(const Affine2D& pBoxelToPixel);

// Cizimi boxel dikdortgeniyle kirpar, bitince eski scissor durumunu geri
// koyar; X-Plane'in kutusu varsa kesisim alinir.
class ScissorGuard
{
public:
    ScissorGuard(const Affine2D& pBoxelToPixel, const RectD& pBoxels);
    ~ScissorGuard();

    ScissorGuard(const ScissorGuard&) = delete;
    ScissorGuard& operator=(const ScissorGuard&) = delete;

    const RectD& pixelRect() const;

private:
    bool mWasEnabled = false;
    std::array<int, 4> mOldBox = {0, 0, 0, 0};
    RectD mPixelRect;
};
#endif  // PRIME_MAP_SRC_GFX_GLSCREEN_H
