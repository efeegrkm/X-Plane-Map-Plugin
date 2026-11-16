// Harita sembolleri icin ortak olculer ve etiket yerlestirme yardimcilari.
// Semboller iki katmanla cizilir: once kalin koyu kontur, ustune renkli cizgi.
#pragma once

#include <string_view>

#include "Color.h"

class LabelPlacer;

namespace XMapSymbols
{

const float kLineWidth = 1.5f;     // sembol cizgisi (boxel)
const float kOutlineExtra = 2.0f;  // kontur, cizgiden bu kadar kalin

// Etiketi sembolun sagina, yer yoksa soluna koymayi dener (pGap: sembolden
// uzaklik). Yer bulunduysa golgeli yazar ve true doner. pShadow: temanin halesi.
bool drawLabelNear(LabelPlacer& pPlacer, float pX, float pY, float pGap, std::string_view pText,
                   const Rgba& pColor, const Rgba& pShadow);

bool drawLabelCentered(LabelPlacer& pPlacer, float pX, float pY, std::string_view pText, const Rgba& pColor,
                       const Rgba& pShadow);

}
