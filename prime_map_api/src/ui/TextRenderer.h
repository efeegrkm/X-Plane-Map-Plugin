// Yazi cizici arayuzu.
#ifndef PRIME_MAP_SRC_UI_TEXTRENDERER_H
#define PRIME_MAP_SRC_UI_TEXTRENDERER_H

#include <string_view>

#include "Color.h"

class TextRenderer
{
public:
    virtual ~TextRenderer() = default;

    virtual void draw(int pX, int pY, std::string_view pText, const Rgba& pColor) = 0;
    virtual int width(std::string_view pText) = 0;
    virtual int height() = 0;
};
#endif
