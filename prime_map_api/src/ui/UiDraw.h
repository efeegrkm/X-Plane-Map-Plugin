// Arayuz cizim yardimcilari ve ortak renk paleti.
#ifndef PRIME_MAP_SRC_UI_UIDRAW_H
#define PRIME_MAP_SRC_UI_UIDRAW_H

#include <string_view>

#include "Color.h"
#include "ScreenMath.h"

namespace Gfx
{
class Canvas;
}

class TextRenderer;

namespace Ui
{
const Rgba kStripBg = {0.06f, 0.07f, 0.09f, 0.72f};
const Rgba kPanelBg = {0.06f, 0.07f, 0.09f, 0.86f};
const Rgba kBorder = {0.30f, 0.33f, 0.38f, 0.90f};
const Rgba kButton = {0.17f, 0.19f, 0.23f, 0.92f};
const Rgba kButtonHover = {0.27f, 0.30f, 0.36f, 0.95f};
const Rgba kButtonActive = {0.00f, 0.48f, 0.70f, 0.95f};
const Rgba kButtonActiveBorder = {0.45f, 0.85f, 1.00f, 1.00f};
const Rgba kButtonDisabled = {0.11f, 0.12f, 0.14f, 0.85f};
const Rgba kTextWhite = {0.96f, 0.97f, 0.98f, 1.0f};
const Rgba kTextGrey = {0.62f, 0.66f, 0.72f, 1.0f};
const Rgba kTextDim = {0.40f, 0.43f, 0.48f, 1.0f};
const Rgba kTextCyan = {0.45f, 0.85f, 1.00f, 1.0f};
const Rgba kTextYellow = {1.00f, 0.80f, 0.25f, 1.0f};

void fillRect(Gfx::Canvas& pCanvas, const RectI& pRect, const Rgba& pColor);
void frameRect(Gfx::Canvas& pCanvas, const RectI& pRect, const Rgba& pColor);
void line(Gfx::Canvas& pCanvas, const PointF& pFrom, const PointF& pTo, float pWidth, const Rgba& pColor);

void setTextRenderer(TextRenderer* pRenderer);
TextRenderer* currentTextRenderer();

void text(int pX, int pY, std::string_view pText, const Rgba& pColor);
int textWidth(std::string_view pText);
int fontHeight();

void textCentered(const RectI& pRect, std::string_view pText, const Rgba& pColor);

const Rgba kShadowBlack = {0.0f, 0.0f, 0.0f, 1.0f};
void textShadowed(int pX, int pY, std::string_view pText, const Rgba& pColor, const Rgba& pShadow = kShadowBlack);

int buttonWidth(std::string_view pLabel);

void Button(Gfx::Canvas& pCanvas, const RectI& pRect, std::string_view pLabel, bool pActive, bool pEnabled,
            bool pHover);
void checkBox(Gfx::Canvas& pCanvas, const RectI& pBox, bool pChecked, bool pHover);
}
#endif
