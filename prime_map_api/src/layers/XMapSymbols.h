// Harita sembollerinin ortak olculeri ve cizim yardimcilari.
#ifndef PRIME_MAP_SRC_LAYERS_XMAPSYMBOLS_H
#define PRIME_MAP_SRC_LAYERS_XMAPSYMBOLS_H

#include <string_view>

#include "Color.h"

class LabelPlacer;

namespace XMapSymbols
{
const float kLineWidth = 1.5f;
const float kOutlineExtra = 2.0f;

bool drawLabelNear(LabelPlacer& pPlacer, float pX, float pY, float pGap, std::string_view pText,
                   const Rgba& pColor, const Rgba& pShadow);

bool drawLabelCentered(LabelPlacer& pPlacer, float pX, float pY, std::string_view pText, const Rgba& pColor,
                       const Rgba& pShadow);
}
#endif
