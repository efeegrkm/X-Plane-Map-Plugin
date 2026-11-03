// UiDraw.cpp
#include "ui/UiDraw.h"

#include <cstdio>
#include <cstring>

#include "XPLMGraphics.h"

#include "GLInclude.h"

namespace
{

const int kButtonPadding = 10;  // yazinin iki yanindaki bosluk

void SetColor(const Rgba& pColor)
{
    glColor4f(pColor.mR, pColor.mG, pColor.mB, pColor.mA);
}

}  // namespace

namespace Ui
{

void UseFlatState()
{
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);
}

void FillRect(const RectI& pRect, const Rgba& pColor)
{
    UseFlatState();
    SetColor(pColor);
    glBegin(GL_QUADS);
    glVertex2i(pRect.mLeft, pRect.mBottom);
    glVertex2i(pRect.mRight, pRect.mBottom);
    glVertex2i(pRect.mRight, pRect.mTop);
    glVertex2i(pRect.mLeft, pRect.mTop);
    glEnd();
}

void FrameRect(const RectI& pRect, const Rgba& pColor)
{
    UseFlatState();
    SetColor(pColor);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(pRect.mLeft + 0.5f, pRect.mBottom + 0.5f);
    glVertex2f(pRect.mRight - 0.5f, pRect.mBottom + 0.5f);
    glVertex2f(pRect.mRight - 0.5f, pRect.mTop - 0.5f);
    glVertex2f(pRect.mLeft + 0.5f, pRect.mTop - 0.5f);
    glEnd();
}

void Line(float pX0, float pY0, float pX1, float pY1, float pWidth, const Rgba& pColor)
{
    UseFlatState();
    SetColor(pColor);
    glLineWidth(pWidth);
    glBegin(GL_LINES);
    glVertex2f(pX0, pY0);
    glVertex2f(pX1, pY1);
    glEnd();
    glLineWidth(1.0f);
}

void Text(int pX, int pY, const char* pText, const Rgba& pColor)
{
    // SDK surumleri arasinda char* / const char* farki var: her zaman
    // degistirilebilir bir kopya veriyoruz.
    char tBuffer[256];
    std::snprintf(tBuffer, sizeof(tBuffer), "%s", pText);
    float tColor[3] = {pColor.mR, pColor.mG, pColor.mB};
    XPLMDrawString(tColor, pX, pY, tBuffer, nullptr, xplmFont_Proportional);
}

int TextWidth(const char* pText)
{
    char tBuffer[256];
    std::snprintf(tBuffer, sizeof(tBuffer), "%s", pText);
    return static_cast<int>(
        XPLMMeasureString(xplmFont_Proportional, tBuffer, static_cast<int>(std::strlen(tBuffer))));
}

int FontHeight()
{
    int tHeight = 10;
    XPLMGetFontDimensions(xplmFont_Proportional, nullptr, &tHeight, nullptr);
    return tHeight;
}

void TextCentered(const RectI& pRect, const char* pText, const Rgba& pColor)
{
    const int tX = (pRect.mLeft + pRect.mRight) / 2 - TextWidth(pText) / 2;
    const int tY = (pRect.mTop + pRect.mBottom) / 2 - FontHeight() / 2 + 1;
    Text(tX, tY, pText, pColor);
}

void TextShadowed(int pX, int pY, const char* pText, const Rgba& pColor)
{
    const Rgba tShadow = {0.0f, 0.0f, 0.0f, 1.0f};
    Text(pX + 1, pY - 1, pText, tShadow);
    Text(pX, pY, pText, pColor);
}

int ButtonWidth(const char* pLabel)
{
    return TextWidth(pLabel) + 2 * kButtonPadding;
}

void Button(const RectI& pRect, const char* pLabel, bool pActive, bool pEnabled, bool pHover)
{
    if (pActive)
    {
        FillRect(pRect, kButtonActive);
        FrameRect(pRect, kButtonActiveBorder);
    }
    else if (!pEnabled)
    {
        FillRect(pRect, kButtonDisabled);
        FrameRect(pRect, kBorder);
    }
    else
    {
        FillRect(pRect, pHover ? kButtonHover : kButton);
        FrameRect(pRect, kBorder);
    }
    TextCentered(pRect, pLabel, pEnabled ? kTextWhite : kTextDim);
}

void CheckBox(const RectI& pBox, bool pChecked, bool pHover)
{
    FillRect(pBox, pHover ? kButtonHover : kButton);
    FrameRect(pBox, pChecked ? kButtonActiveBorder : kBorder);
    if (pChecked)
    {
        RectI tInner;
        tInner.mLeft = pBox.mLeft + 3;
        tInner.mRight = pBox.mRight - 3;
        tInner.mBottom = pBox.mBottom + 3;
        tInner.mTop = pBox.mTop - 3;
        FillRect(tInner, kButtonActive);
    }
}

}  // namespace Ui
