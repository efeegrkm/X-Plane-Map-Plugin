#include "layers/MapSymbols.h"

#include <array>
#include <cmath>

#include "LabelPlacer.h"
#include "ui/UiDraw.h"

namespace
{

// Yazinin kapladigi kutu (biraz pay ile): seyreltme bu kutuyla yapilir
RectD textBox(int pTextX, int pBaseY, int pWidth, int pHeight)
{
    RectD tBox;
    tBox.mLeft = pTextX - 1;
    tBox.mRight = pTextX + pWidth + 2;
    tBox.mBottom = pBaseY - 3;
    tBox.mTop = pBaseY + pHeight + 1;
    return tBox;
}

}

namespace MapSymbols
{

bool drawLabelNear(LabelPlacer& pPlacer, float pX, float pY, float pGap, std::string_view pText,
                   const Rgba& pColor, const Rgba& pShadow)
{
    const int tWidth = Ui::textWidth(pText);
    const int tHeight = Ui::fontHeight();
    const int tBaseY = static_cast<int>(std::floor(pY)) - tHeight / 2 + 1;

    const std::array<int, 2> tCandidatesX = {static_cast<int>(std::floor(pX + pGap)),
                                             static_cast<int>(std::floor(pX - pGap)) - tWidth};
    for (int tTextX : tCandidatesX)
    {
        if (pPlacer.tryPlace(textBox(tTextX, tBaseY, tWidth, tHeight)))
        {
            Ui::textShadowed(tTextX, tBaseY, pText, pColor, pShadow);
            return true;
        }
    }
    return false;
}

bool drawLabelCentered(LabelPlacer& pPlacer, float pX, float pY, std::string_view pText, const Rgba& pColor,
                       const Rgba& pShadow)
{
    const int tWidth = Ui::textWidth(pText);
    const int tHeight = Ui::fontHeight();
    const int tTextX = static_cast<int>(std::floor(pX)) - tWidth / 2;
    const int tBaseY = static_cast<int>(std::floor(pY)) - tHeight / 2 + 1;
    if (!pPlacer.tryPlace(textBox(tTextX, tBaseY, tWidth, tHeight)))
    {
        return false;
    }
    Ui::textShadowed(tTextX, tBaseY, pText, pColor, pShadow);
    return true;
}

}
