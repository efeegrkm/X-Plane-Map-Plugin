// Boxel ile piksel arasindaki donusum ve kirpma (scissor) yardimcilari.
#ifndef PRIME_MAP_SRC_GFX_GLSCREEN_H
#define PRIME_MAP_SRC_GFX_GLSCREEN_H

#include <array>
#include <optional>

#include "ScreenMath.h"

std::optional<Affine2D> readBoxelToPixel();

double pixelsPerBoxel(const Affine2D& pBoxelToPixel);

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
#endif
