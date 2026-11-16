// RGBA renk (her bilesen 0..1). X-Plane'e bagimli DEGIL.
#pragma once

struct Rgba
{
    float mR = 0.0f;
    float mG = 0.0f;
    float mB = 0.0f;
    float mA = 1.0f;
};

inline Rgba withAlpha(const Rgba& pColor, float pAlpha)
{
    return {pColor.mR, pColor.mG, pColor.mB, pAlpha};
}
