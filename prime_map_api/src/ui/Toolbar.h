// Haritanin ust kenarindaki arac seridi.
#ifndef PRIME_MAP_SRC_UI_TOOLBAR_H
#define PRIME_MAP_SRC_UI_TOOLBAR_H

#include <vector>

#include "Drawings.h"
#include "ScreenMath.h"

namespace Gfx
{
class Canvas;
}

enum class ToolbarAction
{
    None,
    Layers,
    XMapMode,
    FollowAircraft,
    CenterAircraft,
    goToAirport,
    drawTools,
    ModePan,
    ModePoint,
    ModeRoute,
    ModeRegion,
    Finish,
    Undo,
    Clear,
    popOut,
    HideControls,
};

struct ToolbarState
{
    bool mLayersOpen = false;
    bool mModesOpen = false;
    bool mSearchOpen = false;
    const char* mModeLabel = "XMap: Scenery";
    bool mFollowing = false;
    bool mHasAircraft = false;
    bool mPoppedOut = false;
    bool mToolsOpen = false;
    Drawings::Mode mMode = Drawings::Mode::Pan;
    bool mCanFinish = false;
    bool mCanUndo = false;
};

class Toolbar
{
public:
    static const int kRowHeight = 28;

    void layout(const RectI& pArea, const ToolbarState& pState);

    void draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, const ToolbarState& pState) const;

    ToolbarAction hitTest(int pX, int pY, const ToolbarState& pState) const;

    bool contains(int pX, int pY) const;
    const RectI& strip() const;
    int height() const;

    RectI buttonRect(ToolbarAction pAction) const;

private:
    struct Item
    {
        ToolbarAction mAction;
        const char* mLabel;
        RectI mRect;
    };

    int placeRow(const std::vector<ToolbarAction>& pActions, int pX, int pTop, bool pRightToLeft,
                 const ToolbarState& pState);
    static int rowWidth(const std::vector<ToolbarAction>& pActions, const ToolbarState& pState);
    static bool isActive(ToolbarAction pAction, const ToolbarState& pState);
    static bool isEnabled(ToolbarAction pAction, const ToolbarState& pState);
    static const char* labelFor(ToolbarAction pAction, const ToolbarState& pState);

    std::vector<Item> mItems;
    std::vector<int> mDividers;
    RectI mStrip;
};
#endif
