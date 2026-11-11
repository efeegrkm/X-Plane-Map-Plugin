// TextRenderer.h
// Yazi cizici arayuzu (0.18). Ui::Text / TextWidth / FontHeight bunun
// uzerinden calisir:
//
//   AtlasTextRenderer : kendi atlasimiz (DejaVuSans, stb_truetype); harfler
//                       Canvas'ta biriktirilir, tek cizim cagrisiyla basilir.
//                       FBO'ya da dogru cizer. Varsayilan.
//   (yedek)           : X-Plane'in XPLMDrawString'i. Atlas kurulamazsa ya da
//                       map_settings.ini [text] renderer = xplane ise.
//
// Koordinatlar boxel; pY yazinin taban cizgisi (XPLMDrawString ile ayni kural).
#pragma once

#include <string_view>

#include "Color.h"

class TextRenderer
{
public:
    virtual ~TextRenderer() = default;

    virtual void Draw(int pX, int pY, std::string_view pText, const Rgba& pColor) = 0;
    virtual int Width(std::string_view pText) = 0;  // boxel
    virtual int Height() = 0;                        // buyuk harf yuksekligi (boxel)
};
