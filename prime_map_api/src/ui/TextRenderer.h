// Yazi cizici arayuzu; Ui::text / textWidth / fontHeight bunun uzerinden calisir.
#pragma once

#include <string_view>

#include "Color.h"

class TextRenderer
{
public:
    virtual ~TextRenderer() = default;

    virtual void draw(int pX, int pY, std::string_view pText, const Rgba& pColor) = 0;
    virtual int width(std::string_view pText) = 0;  // boxel
    virtual int height() = 0;                        // buyuk harf yuksekligi (boxel)
};
