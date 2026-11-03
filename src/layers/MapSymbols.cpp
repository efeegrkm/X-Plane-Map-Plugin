// MapSymbols.cpp
#include "layers/MapSymbols.h"

#include <cmath>

#include "GLInclude.h"
#include "LabelPlacer.h"

namespace
{

const float kPi = 3.14159265f;

float Radians(float pDegrees)
{
    return pDegrees * kPi / 180.0f;
}

}  // namespace

namespace MapSymbols
{

void SetColor(const Rgba& pColor)
{
    glColor4f(pColor.mR, pColor.mG, pColor.mB, pColor.mA);
}

void EmitPolygonEdges(float pX, float pY, float pRadius, int pSides, float pStartDeg)
{
    for (int tSide = 0; tSide < pSides; ++tSide)
    {
        const float tAngle0 = Radians(pStartDeg) + 2.0f * kPi * tSide / pSides;
        const float tAngle1 = Radians(pStartDeg) + 2.0f * kPi * (tSide + 1) / pSides;
        glVertex2f(pX + pRadius * std::cos(tAngle0), pY + pRadius * std::sin(tAngle0));
        glVertex2f(pX + pRadius * std::cos(tAngle1), pY + pRadius * std::sin(tAngle1));
    }
}

void EmitDisc(float pX, float pY, float pRadius, int pSegments)
{
    for (int tSegment = 0; tSegment < pSegments; ++tSegment)
    {
        const float tAngle0 = 2.0f * kPi * tSegment / pSegments;
        const float tAngle1 = 2.0f * kPi * (tSegment + 1) / pSegments;
        glVertex2f(pX, pY);
        glVertex2f(pX + pRadius * std::cos(tAngle0), pY + pRadius * std::sin(tAngle0));
        glVertex2f(pX + pRadius * std::cos(tAngle1), pY + pRadius * std::sin(tAngle1));
    }
}

void EmitDottedCircle(float pX, float pY, float pRadius, int pDots)
{
    for (int tDot = 0; tDot < pDots; ++tDot)
    {
        const float tAngle = 2.0f * kPi * tDot / pDots;
        glVertex2f(pX + pRadius * std::cos(tAngle), pY + pRadius * std::sin(tAngle));
    }
}

bool DrawLabelNear(LabelPlacer& pPlacer, float pX, float pY, float pGap, const char* pText, const Rgba& pColor)
{
    const int tWidth = Ui::TextWidth(pText);
    const int tHeight = Ui::FontHeight();
    const int tBaseY = static_cast<int>(std::floor(pY)) - tHeight / 2 + 1;

    // Once sag, sonra sol
    const int tCandidatesX[2] = {static_cast<int>(std::floor(pX + pGap)),
                                 static_cast<int>(std::floor(pX - pGap)) - tWidth};
    for (int tTextX : tCandidatesX)
    {
        RectD tBox;
        tBox.mLeft = tTextX - 1;
        tBox.mRight = tTextX + tWidth + 2;
        tBox.mBottom = tBaseY - 3;
        tBox.mTop = tBaseY + tHeight + 1;
        if (pPlacer.TryPlace(tBox))
        {
            Ui::TextShadowed(tTextX, tBaseY, pText, pColor);
            return true;
        }
    }
    return false;
}

bool DrawLabelCentered(LabelPlacer& pPlacer, float pX, float pY, const char* pText, const Rgba& pColor)
{
    const int tWidth = Ui::TextWidth(pText);
    const int tHeight = Ui::FontHeight();
    const int tTextX = static_cast<int>(std::floor(pX)) - tWidth / 2;
    const int tBaseY = static_cast<int>(std::floor(pY)) - tHeight / 2 + 1;
    RectD tBox;
    tBox.mLeft = tTextX - 1;
    tBox.mRight = tTextX + tWidth + 2;
    tBox.mBottom = tBaseY - 3;
    tBox.mTop = tBaseY + tHeight + 1;
    if (!pPlacer.TryPlace(tBox))
    {
        return false;
    }
    Ui::TextShadowed(tTextX, tBaseY, pText, pColor);
    return true;
}

}  // namespace MapSymbols
