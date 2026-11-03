// UiDraw.h
// Arayuz cizim yardimcilari: dolu kutu, cerceve, cizgi, yazi, dugme, onay kutusu.
// Tum koordinatlar global boxel (pencere draw callback'inin koordinatlari).
//
// Onemli: XPLMDrawString yazi cizerken grafik durumunu degistirir (doku acar).
// Bu yuzden her geometri yardimcisi once UseFlatState() cagirir; yoksa yazidan
// sonra cizilen kutular dokulu cizilip gorunmez olabilir.
#pragma once

#include "ScreenMath.h"

struct Rgba
{
    float mR;
    float mG;
    float mB;
    float mA;
};

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

// fog yok, doku yok, isik yok, alpha test yok, saydamlik var, derinlik yok
void UseFlatState();

void FillRect(const RectI& pRect, const Rgba& pColor);
void FrameRect(const RectI& pRect, const Rgba& pColor);
void Line(float pX0, float pY0, float pX1, float pY1, float pWidth, const Rgba& pColor);

// pX, pY: yazinin sol alt kosesi (taban cizgisi). Saydamlik desteklenmez.
void Text(int pX, int pY, const char* pText, const Rgba& pColor);
int TextWidth(const char* pText);
int FontHeight();

// Yaziyi kutunun ortasina hizalar
void TextCentered(const RectI& pRect, const char* pText, const Rgba& pColor);

// Harita uzerindeki yazilar icin: once 1 boxel sag-asagiya siyah golge,
// sonra yazi. Her zemin renginde okunur kalir.
void TextShadowed(int pX, int pY, const char* pText, const Rgba& pColor);

// Etiketi sigdiracak dugme genisligi
int ButtonWidth(const char* pLabel);

void Button(const RectI& pRect, const char* pLabel, bool pActive, bool pEnabled, bool pHover);
void CheckBox(const RectI& pBox, bool pChecked, bool pHover);

}  // namespace Ui
