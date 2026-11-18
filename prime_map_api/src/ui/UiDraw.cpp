#include "ui/UiDraw.h"

#include <string>

#include "XPLMGraphics.h"

#include "gfx/Canvas.h"
#include "ui/TextRenderer.h"

namespace
{
const int kButtonPadding = 10;

TextRenderer* gTextRenderer = nullptr;

std::string mutableCopy(std::string_view pText)
{
    return std::string(pText);
}
}

namespace Ui
{
void fillRect(Gfx::Canvas& pCanvas, const RectI& pRect, const Rgba& pColor)
{
    Gfx::Mesh tMesh;
    tMesh.addRect(toRectD(pRect), pColor);
    pCanvas.draw(tMesh);
}

void frameRect(Gfx::Canvas& pCanvas, const RectI& pRect, const Rgba& pColor)
{
    Gfx::Mesh tMesh;
    tMesh.addFrame(toRectD(pRect), 1.0f, pColor);
    pCanvas.draw(tMesh);
}

void line(Gfx::Canvas& pCanvas, const PointF& pFrom, const PointF& pTo, float pWidth, const Rgba& pColor)
{
    Gfx::Mesh tMesh;
    tMesh.addLine(pFrom, pTo, pWidth, pColor);
    pCanvas.draw(tMesh);
}

void setTextRenderer(TextRenderer* pRenderer)
{
    gTextRenderer = pRenderer;
}

TextRenderer* currentTextRenderer()
{
    return gTextRenderer;
}

void text(int pX, int pY, std::string_view pText, const Rgba& pColor)
{
    if (gTextRenderer)
    {
        gTextRenderer->draw(pX, pY, pText, pColor);
        return;
    }
    std::string tCopy = mutableCopy(pText);
    float tColor[3] = {pColor.mR, pColor.mG, pColor.mB};
    XPLMDrawString(tColor, pX, pY, tCopy.data(), nullptr, xplmFont_Proportional);
}

int textWidth(std::string_view pText)
{
    if (gTextRenderer)
    {
        return gTextRenderer->width(pText);
    }
    std::string tCopy = mutableCopy(pText);
    return static_cast<int>(XPLMMeasureString(xplmFont_Proportional, tCopy.data(), static_cast<int>(tCopy.size())));
}

int fontHeight()
{
    if (gTextRenderer)
    {
        return gTextRenderer->height();
    }
    int tHeight = 10;
    XPLMGetFontDimensions(xplmFont_Proportional, nullptr, &tHeight, nullptr);
    return tHeight;
}

void textCentered(const RectI& pRect, std::string_view pText, const Rgba& pColor)
{
    const int tX = (pRect.mLeft + pRect.mRight) / 2 - textWidth(pText) / 2;
    const int tY = (pRect.mTop + pRect.mBottom) / 2 - fontHeight() / 2 + 1;
    text(tX, tY, pText, pColor);
}

void textShadowed(int pX, int pY, std::string_view pText, const Rgba& pColor, const Rgba& pShadow)
{
    text(pX + 1, pY - 1, pText, pShadow);
    text(pX, pY, pText, pColor);
}

int buttonWidth(std::string_view pLabel)
{
    return textWidth(pLabel) + 2 * kButtonPadding;
}

void Button(Gfx::Canvas& pCanvas, const RectI& pRect, std::string_view pLabel, bool pActive, bool pEnabled,
            bool pHover)
{
    Rgba tFill = pHover ? kButtonHover : kButton;
    Rgba tBorder = kBorder;
    if (pActive)
    {
        tFill = kButtonActive;
        tBorder = kButtonActiveBorder;
    }
    else if (!pEnabled)
    {
        tFill = kButtonDisabled;
    }
    Gfx::Mesh tMesh;
    tMesh.addRect(toRectD(pRect), tFill);
    tMesh.addFrame(toRectD(pRect), 1.0f, tBorder);
    pCanvas.draw(tMesh);
    textCentered(pRect, pLabel, pEnabled ? kTextWhite : kTextDim);
}

void checkBox(Gfx::Canvas& pCanvas, const RectI& pBox, bool pChecked, bool pHover)
{
    Gfx::Mesh tMesh;
    tMesh.addRect(toRectD(pBox), pHover ? kButtonHover : kButton);
    tMesh.addFrame(toRectD(pBox), 1.0f, pChecked ? kButtonActiveBorder : kBorder);
    if (pChecked)
    {
        RectD tInner = toRectD(pBox);
        tInner.mLeft += 3.0;
        tInner.mRight -= 3.0;
        tInner.mBottom += 3.0;
        tInner.mTop -= 3.0;
        tMesh.addRect(tInner, kButtonActive);
    }
    pCanvas.draw(tMesh);
}
}
