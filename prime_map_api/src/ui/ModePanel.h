// ModePanel.h
// "Map: ..." dugmesinin altinda acilan harita modu listesi (tek secim).
//
//   +------------+
//   | MAP MODE   |
//   | (o) Scenery|
//   | ( ) VFR    |
//   | ( ) IFR Low|
//   | ( ) IFR High
//   +------------+
//
// Panel sadece cizer ve tiklanan modu soyler; modu degistirmek MapWindow'un isi.
#pragma once

#include <optional>

#include "MapTheme.h"
#include "ScreenMath.h"

namespace Gfx
{
class Canvas;
}

class ModePanel
{
public:
    bool IsOpen() const;
    void SetOpen(bool pOpen);
    void Toggle();

    // pAnchor: "Map: ..." dugmesi; panel onun altina acilir
    void Layout(const RectI& pAnchor);
    void Draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, MapMode pCurrent) const;

    // (pX, pY) panelin ustundeyse true; bir moda denk geldiyse pOutMode dolar
    bool HitTest(int pX, int pY, std::optional<MapMode>* pOutMode) const;

private:
    RectI RowRect(int pIndex) const;

    bool mOpen = false;
    RectI mRect;
};
