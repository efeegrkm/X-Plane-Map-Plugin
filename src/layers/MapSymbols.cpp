// MapSymbols.cpp
#include "layers/MapSymbols.h"

#include <array>
#include <cmath>

#include "LabelPlacer.h"
#include "ui/UiDraw.h"

namespace
{

// Yazinin kapladigi kutu (biraz pay ile): seyreltme bu kutuyla yapilir
RectD TextBox(int pTextX, int pBaseY, int pWidth, int pHeight)
{
    RectD tBox;
    tBox.mLeft = pTextX - 1;
    tBox.mRight = pTextX + pWidth + 2;
    tBox.mBottom = pBaseY - 3;
    tBox.mTop = pBaseY + pHeight + 1;
    return tBox;
}

}  // namespace

namespace MapSymbols
{

bool DrawLabelNear(LabelPlacer& pPlacer, float pX, float pY, float pGap, std::string_view pText,
                   const Rgba& pColor)
{
    const int tWidth = Ui::TextWidth(pText);
    const int tHeight = Ui::FontHeight();
    const int tBaseY = static_cast<int>(std::floor(pY)) - tHeight / 2 + 1;

    // Once sag, sonra sol
    const std::array<int, 2> tCandidatesX = {static_cast<int>(std::floor(pX + pGap)),
                                             static_cast<int>(std::floor(pX - pGap)) - tWidth};
    for (int tTextX : tCandidatesX)
    {
        if (pPlacer.TryPlace(TextBox(tTextX, tBaseY, tWidth, tHeight)))
        {
            Ui::TextShadowed(tTextX, tBaseY, pText, pColor);
            return true;
        }
    }
    return false;
}

bool DrawLabelCentered(LabelPlacer& pPlacer, float pX, float pY, std::string_view pText, const Rgba& pColor)
{
    const int tWidth = Ui::TextWidth(pText);
    const int tHeight = Ui::FontHeight();
    const int tTextX = static_cast<int>(std::floor(pX)) - tWidth / 2;
    const int tBaseY = static_cast<int>(std::floor(pY)) - tHeight / 2 + 1;
    if (!pPlacer.TryPlace(TextBox(tTextX, tBaseY, tWidth, tHeight)))
    {
        return false;
    }
    Ui::TextShadowed(tTextX, tBaseY, pText, pColor);
    return true;
}

}  // namespace MapSymbols
