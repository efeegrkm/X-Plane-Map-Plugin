// "XMap: ..." dugmesinin altinda acilan harita modu listesi (tek secim).
// Panel sadece cizer ve tiklanan modu soyler; modu degistirmek XMapWindow'un isi.
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
    bool isOpen() const;
    void setOpen(bool pOpen);
    void toggle();

    // pAnchor: "XMap: ..." dugmesi; panel onun altina acilir
    void layout(const RectI& pAnchor);
    void draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, MapMode pCurrent) const;

    // (pX, pY) panelin ustundeyse true; bir moda denk geldiyse pOutMode dolar
    bool hitTest(int pX, int pY, std::optional<MapMode>* pOutMode) const;

private:
    RectI rowRect(int pIndex) const;

    bool mOpen = false;
    RectI mRect;
};
