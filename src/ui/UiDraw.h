// UiDraw.h
// Arayuz cizim yardimcilari: dolu kutu, cerceve, cizgi, yazi, dugme, onay kutusu.
// Tum koordinatlar global boxel (pencere draw callback'inin koordinatlari).
//
// Sekiller Gfx::Canvas ile (modern OpenGL). Yazilar etkin TextRenderer ile
// (0.18: AtlasTextRenderer, kendi atlasimiz; yoksa X-Plane'in XPLMDrawString'i).
// Yedek yol Turkce harf ve derece isareti cizemez: metinler simdilik ASCII.
#pragma once

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

// --- Renkler (yari saydam koyu tema) ---
const Rgba kStripBg = {0.06f, 0.07f, 0.09f, 0.72f};    // ust/alt serit zemini
const Rgba kPanelBg = {0.06f, 0.07f, 0.09f, 0.86f};    // acilir panel zemini
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

void FillRect(Gfx::Canvas& pCanvas, const RectI& pRect, const Rgba& pColor);
void FrameRect(Gfx::Canvas& pCanvas, const RectI& pRect, const Rgba& pColor);
void Line(Gfx::Canvas& pCanvas, const PointF& pFrom, const PointF& pTo, float pWidth, const Rgba& pColor);

// Etkin yazi cizici. nullptr: X-Plane'in XPLMDrawString'i (yedek). Sahibi MapWindow / onizleme;
// X-Plane'in yazi fonksiyonlari gibi surec genelinde tektir.
void SetTextRenderer(TextRenderer* pRenderer);

// pX, pY: yazinin sol alt kosesi (taban cizgisi). Yedek yolda saydamlik desteklenmez.
void Text(int pX, int pY, std::string_view pText, const Rgba& pColor);
int TextWidth(std::string_view pText);
int FontHeight();

// Yaziyi kutunun ortasina hizalar
void TextCentered(const RectI& pRect, std::string_view pText, const Rgba& pColor);

// Harita uzerindeki yazilar icin: once 1 boxel sag-asagiya golge (varsayilan
// siyah; acik zeminli modda beyaz), sonra yazi. Her zemin renginde okunur kalir.
const Rgba kShadowBlack = {0.0f, 0.0f, 0.0f, 1.0f};
void TextShadowed(int pX, int pY, std::string_view pText, const Rgba& pColor, const Rgba& pShadow = kShadowBlack);

// Etiketi sigdiracak dugme genisligi
int ButtonWidth(std::string_view pLabel);

void Button(Gfx::Canvas& pCanvas, const RectI& pRect, std::string_view pLabel, bool pActive, bool pEnabled,
            bool pHover);
void CheckBox(Gfx::Canvas& pCanvas, const RectI& pBox, bool pChecked, bool pHover);

}  // namespace Ui
