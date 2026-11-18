// Tasinabilir cizim araclari paleti.
#ifndef PRIME_MAP_SRC_UI_TOOLPALETTE_H
#define PRIME_MAP_SRC_UI_TOOLPALETTE_H

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
        bool mInside = false;
        ToolbarAction mAction = ToolbarAction::None;
        bool mStartDrag = false;
        bool mToggleCollapse = false;
    };

    bool isVisible() const;
    void setVisible(bool pVisible);
    bool isCollapsed() const;
    void setCollapsed(bool pCollapsed);

    int offsetRight() const;
    int offsetTop() const;
    void setOffset(int pRight, int pTop);

    void layout(const RectI& pBounds, const ToolbarState& pState);

    void draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, const ToolbarState& pState) const;
    Hit hitTest(int pX, int pY, const ToolbarState& pState) const;
    bool contains(int pX, int pY) const;

    void moveBy(int pDeltaX, int pDeltaY);

private:
    struct Item
    {
        ToolbarAction mAction;
        RectI mRect;
        bool mIsTool;
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
    int mSeparatorY = 0;
    std::vector<Item> mItems;
};
#endif
