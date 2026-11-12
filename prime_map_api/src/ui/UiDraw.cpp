// UiDraw.cpp
#include "ui/UiDraw.h"

#include <string>

#include "XPLMGraphics.h"

#include "gfx/Canvas.h"
#include "ui/TextRenderer.h"

namespace
{

const int kButtonPadding = 10;  // yazinin iki yanindaki bosluk

TextRenderer* gTextRenderer = nullptr;  // etkin yazi cizici (SetTextRenderer); nullptr = XPLMDrawString

// XPLMDrawString / XPLMMeasureString degistirilebilir char* istiyor:
// her seferinde kendi kopyamizi veriyoruz.
std::string MutableCopy(std::string_view pText)
{
    return std::string(pText);
}

}  // namespace

namespace Ui
{

void FillRect(Gfx::Canvas& pCanvas, const RectI& pRect, const Rgba& pColor)
{
    Gfx::Mesh tMesh;
    tMesh.AddRect(ToRectD(pRect), pColor);
    pCanvas.Draw(tMesh);
}

void FrameRect(Gfx::Canvas& pCanvas, const RectI& pRect, const Rgba& pColor)
{
    Gfx::Mesh tMesh;
    tMesh.AddFrame(ToRectD(pRect), 1.0f, pColor);
    pCanvas.Draw(tMesh);
}

void Line(Gfx::Canvas& pCanvas, const PointF& pFrom, const PointF& pTo, float pWidth, const Rgba& pColor)
{
    Gfx::Mesh tMesh;
    tMesh.AddLine(pFrom, pTo, pWidth, pColor);
    pCanvas.Draw(tMesh);
}

void SetTextRenderer(TextRenderer* pRenderer)
{
    gTextRenderer = pRenderer;
}

TextRenderer* CurrentTextRenderer()
{
    return gTextRenderer;
}

void Text(int pX, int pY, std::string_view pText, const Rgba& pColor)
{
    if (gTextRenderer)
    {
        gTextRenderer->Draw(pX, pY, pText, pColor);
        return;
    }
    std::string tCopy = MutableCopy(pText);
    float tColor[3] = {pColor.mR, pColor.mG, pColor.mB};
    XPLMDrawString(tColor, pX, pY, tCopy.data(), nullptr, xplmFont_Proportional);
}

int TextWidth(std::string_view pText)
{
    if (gTextRenderer)
    {
        return gTextRenderer->Width(pText);
    }
    std::string tCopy = MutableCopy(pText);
    return static_cast<int>(XPLMMeasureString(xplmFont_Proportional, tCopy.data(), static_cast<int>(tCopy.size())));
}

int FontHeight()
{
    if (gTextRenderer)
    {
        return gTextRenderer->Height();
    }
    int tHeight = 10;
    XPLMGetFontDimensions(xplmFont_Proportional, nullptr, &tHeight, nullptr);
    return tHeight;
}

void TextCentered(const RectI& pRect, std::string_view pText, const Rgba& pColor)
{
    const int tX = (pRect.mLeft + pRect.mRight) / 2 - TextWidth(pText) / 2;
    const int tY = (pRect.mTop + pRect.mBottom) / 2 - FontHeight() / 2 + 1;
    Text(tX, tY, pText, pColor);
}

void TextShadowed(int pX, int pY, std::string_view pText, const Rgba& pColor, const Rgba& pShadow)
{
    Text(pX + 1, pY - 1, pText, pShadow);
    Text(pX, pY, pText, pColor);
}

int ButtonWidth(std::string_view pLabel)
{
    return TextWidth(pLabel) + 2 * kButtonPadding;
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
    tMesh.AddRect(ToRectD(pRect), tFill);
    tMesh.AddFrame(ToRectD(pRect), 1.0f, tBorder);
    pCanvas.Draw(tMesh);
    TextCentered(pRect, pLabel, pEnabled ? kTextWhite : kTextDim);
}

void CheckBox(Gfx::Canvas& pCanvas, const RectI& pBox, bool pChecked, bool pHover)
{
    Gfx::Mesh tMesh;
    tMesh.AddRect(ToRectD(pBox), pHover ? kButtonHover : kButton);
    tMesh.AddFrame(ToRectD(pBox), 1.0f, pChecked ? kButtonActiveBorder : kBorder);
    if (pChecked)
    {
        RectD tInner = ToRectD(pBox);
        tInner.mLeft += 3.0;
        tInner.mRight -= 3.0;
        tInner.mBottom += 3.0;
        tInner.mTop -= 3.0;
        tMesh.AddRect(tInner, kButtonActive);
    }
    pCanvas.Draw(tMesh);
}

}  // namespace Ui
