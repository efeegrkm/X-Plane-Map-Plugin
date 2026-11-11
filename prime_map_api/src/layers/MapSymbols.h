// MapSymbols.h
// Harita sembolleri icin ortak olculer ve etiket yerlestirme yardimcilari.
//
// Semboller okunurluk icin iki katmanla cizilir: once kalin koyu kontur,
// ustune ince renkli cizgi (Gfx::OutlinedLines bunu kendisi yapar).
#pragma once

#include <string_view>

#include "Color.h"

class LabelPlacer;

namespace MapSymbols
{

const float kLineWidth = 1.5f;     // sembol cizgisi (boxel)
const float kOutlineExtra = 2.0f;  // kontur, cizgiden bu kadar kalin

// Etiketi sembolun sagina, yer yoksa soluna koymayi dener (pGap: sembolden
// uzaklik). Yer bulunduysa yaziyi golgeli cizer ve true doner.
// pShadow: yazi golgesi (temanin halesi, MapTheme::mHalo tam opak).
bool DrawLabelNear(LabelPlacer& pPlacer, float pX, float pY, float pGap, std::string_view pText,
                   const Rgba& pColor, const Rgba& pShadow);

// Etiketi tam (pX, pY) noktasina ortalayarak koymayi dener (pist numarasi gibi)
bool DrawLabelCentered(LabelPlacer& pPlacer, float pX, float pY, std::string_view pText, const Rgba& pColor,
                       const Rgba& pShadow);

}  // namespace MapSymbols
