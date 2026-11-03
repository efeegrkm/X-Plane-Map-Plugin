// MapSymbols.h
// Harita sembolleri icin ortak cizim yardimcilari (boxel koordinatinda).
//
// Emit... fonksiyonlari sadece kose noktasi uretir; glBegin/glEnd'i cagiran
// yapar. Boylece yuzlerce sembol tek bir glBegin/glEnd icinde (toplu) cizilir.
// Okunurluk icin semboller iki gecisle cizilir: once kalin koyu kontur,
// sonra ince renkli cizgi (bkz. NavaidLayer::Draw).
#pragma once

#include "ui/UiDraw.h"

class LabelPlacer;

namespace MapSymbols
{

const Rgba kOutline = {0.0f, 0.0f, 0.0f, 0.70f};  // sembollerin altindaki koyu kontur
const float kOutlineWidth = 3.5f;
const float kLineWidth = 1.5f;

void SetColor(const Rgba& pColor);

// GL_LINES icinde: pSides kenarli duzgun cokgenin kenarlari.
// pStartDeg: ilk kosenin acisi (0 = sag, 90 = yukari).
void EmitPolygonEdges(float pX, float pY, float pRadius, int pSides, float pStartDeg);

// GL_TRIANGLES icinde: dolu daire
void EmitDisc(float pX, float pY, float pRadius, int pSegments);

// GL_POINTS icinde: cember uzerinde esit aralikli noktalar (NDB sembolu)
void EmitDottedCircle(float pX, float pY, float pRadius, int pDots);

// Etiketi sembolun sagina, yer yoksa soluna koymayi dener (pGap: sembolden
// uzaklik). Yer bulunduysa yaziyi golgeli cizer ve true doner.
bool DrawLabelNear(LabelPlacer& pPlacer, float pX, float pY, float pGap, const char* pText, const Rgba& pColor);

// Etiketi tam (pX, pY) noktasina ortalayarak koymayi dener (pist numarasi gibi)
bool DrawLabelCentered(LabelPlacer& pPlacer, float pX, float pY, const char* pText, const Rgba& pColor);

}  // namespace MapSymbols
