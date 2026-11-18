// Harita modu secim paneli.
#ifndef PRIME_MAP_SRC_UI_MODEPANEL_H
#define PRIME_MAP_SRC_UI_MODEPANEL_H

#include <optional>

#include "XMapTheme.h"
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

    void layout(const RectI& pAnchor);
    void draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, XMapMode pCurrent) const;

    bool hitTest(int pX, int pY, std::optional<XMapMode>* pOutMode) const;

private:
    RectI rowRect(int pIndex) const;

    bool mOpen = false;
    RectI mRect;
};
#endif
