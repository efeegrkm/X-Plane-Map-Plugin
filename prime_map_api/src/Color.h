// RGBA renk tipi (bilesenler 0..1).
#ifndef PRIME_MAP_SRC_COLOR_H
#define PRIME_MAP_SRC_COLOR_H

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
#endif
