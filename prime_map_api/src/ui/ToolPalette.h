// Cizim araclari paleti: haritanin ustunde yuzen, tasinabilir ve katlanabilir
// kucuk bir panel (Pan/Point/Route/Region + Finish/Undo/Clear).
#pragma once

#include <vector>

#include "Color.h"
#include "ScreenMath.h"
#include "ui/Toolbar.h"

namespace Gfx
{
class Canvas;
}

class ToolPalette
{
public:
    struct Hit
    {
        bool mInside = false;          // panelin ustunde mi
        ToolbarAction mAction = ToolbarAction::None;
        bool mStartDrag = false;       // baslik cubuguna basildi: surukleme basliyor
        bool mToggleCollapse = false;  // ok: katla / ac
    };

    bool isVisible() const;
    void setVisible(bool pVisible);
    bool isCollapsed() const;
    void setCollapsed(bool pCollapsed);

    // Haritanin sag ust kosesine gore yer (boxel, >= 0); ayar dosyasi icin
    int offsetRight() const;
    int offsetTop() const;
    void setOffset(int pRight, int pTop);

    // pBounds: panelin kalabilecegi alan (ust ve alt seritlerin arasi). Her cizimden once.
    void layout(const RectI& pBounds, const ToolbarState& pState);

    void draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, const ToolbarState& pState) const;
    Hit hitTest(int pX, int pY, const ToolbarState& pState) const;
    bool contains(int pX, int pY) const;

    // Surukleme: fare bu kadar kaydi (Layout bir sonraki karede alana sigdirir)
    void moveBy(int pDeltaX, int pDeltaY);

private:
    struct Item
    {
        ToolbarAction mAction;
        RectI mRect;
        bool mIsTool;  // true: ikonlu arac satiri, false: islem dugmesi
    };

    static const char* labelFor(ToolbarAction pAction);
    static bool isActive(ToolbarAction pAction, const ToolbarState& pState);
    static bool isEnabled(ToolbarAction pAction, const ToolbarState& pState);
    static ToolbarAction activeTool(const ToolbarState& pState);
    static void drawIcon(Gfx::Canvas& pCanvas, ToolbarAction pAction, const PointF& pCenter, const Rgba& pColor);
    RectI chevronRect() const;

    bool mVisible = true;
    bool mCollapsed = false;
    int mOffsetRight = 8;
    int mOffsetTop = 8;
    RectI mRect;
    RectI mHeader;
    int mSeparatorY = 0;  // arac satirlari ile islem dugmeleri arasi cizgi (0: yok)
    std::vector<Item> mItems;
};
