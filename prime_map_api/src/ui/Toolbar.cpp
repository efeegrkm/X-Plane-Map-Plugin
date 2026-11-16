#include "ui/Toolbar.h"

#include "ui/UiDraw.h"

namespace
{

const int kMargin = 4;      // serit kenari ile dugme arasi
const int kGap = 4;         // iki dugme arasi
const int kGroupGap = 14;   // dugme gruplari arasi

// Dugme gruplari (soldan saga). ToolbarAction::None = gruplar arasi ayirici.
const std::vector<ToolbarAction> kGeneralGroup = {
    ToolbarAction::Layers,         ToolbarAction::XMapMode, ToolbarAction::None, ToolbarAction::FollowAircraft,
    ToolbarAction::CenterAircraft, ToolbarAction::goToAirport, ToolbarAction::None, ToolbarAction::drawTools};
const std::vector<ToolbarAction> kWindowGroup = {ToolbarAction::HideControls, ToolbarAction::popOut};  // sagdan sola

}  // namespace

int Toolbar::rowWidth(const std::vector<ToolbarAction>& pActions, const ToolbarState& pState)
{
    int tWidth = 0;
    for (ToolbarAction tAction : pActions)
    {
        tWidth += (tAction == ToolbarAction::None) ? kGroupGap : Ui::buttonWidth(labelFor(tAction, pState)) + kGap;
    }
    return tWidth;
}

int Toolbar::placeRow(const std::vector<ToolbarAction>& pActions, int pX, int pTop, bool pRightToLeft,
                      const ToolbarState& pState)
{
    int tX = pX;
    for (ToolbarAction tAction : pActions)
    {
        if (tAction == ToolbarAction::None)
        {
            // Ayirici: boslugun ortasinda ince cizgi
            const int tGapStart = tX - (pRightToLeft ? -kGap : kGap);
            const int tGapEnd = tX + (pRightToLeft ? -kGroupGap : kGroupGap);
            mDividers.push_back((tGapStart + tGapEnd) / 2);
            tX = tGapEnd;
            continue;
        }
        const char* tLabel = labelFor(tAction, pState);
        const int tWidth = Ui::buttonWidth(tLabel);
        Item tItem;
        tItem.mAction = tAction;
        tItem.mLabel = tLabel;
        tItem.mRect.mTop = pTop - kMargin;
        tItem.mRect.mBottom = pTop - kRowHeight + kMargin;
        if (pRightToLeft)
        {
            tItem.mRect.mRight = tX;
            tItem.mRect.mLeft = tX - tWidth;
            tX = tItem.mRect.mLeft - kGap;
        }
        else
        {
            tItem.mRect.mLeft = tX;
            tItem.mRect.mRight = tX + tWidth;
            tX = tItem.mRect.mRight + kGap;
        }
        mItems.push_back(tItem);
    }
    return tX;
}

void Toolbar::layout(const RectI& pArea, const ToolbarState& pState)
{
    mItems.clear();
    mDividers.clear();
    const int tAvailable = pArea.width() - 2 * kMargin;
    const int tNeeded = rowWidth(kGeneralGroup, pState) + kGroupGap + rowWidth(kWindowGroup, pState);
    const bool tTwoRows = tNeeded > tAvailable;

    mStrip.mLeft = pArea.mLeft;
    mStrip.mRight = pArea.mRight;
    mStrip.mTop = pArea.mTop;
    mStrip.mBottom = pArea.mTop - (tTwoRows ? 2 * kRowHeight - kMargin : kRowHeight);

    placeRow(kGeneralGroup, mStrip.mLeft + kMargin, mStrip.mTop, false, pState);
    const int tWindowTop = tTwoRows ? mStrip.mTop - kRowHeight + kMargin : mStrip.mTop;
    placeRow(kWindowGroup, mStrip.mRight - kMargin, tWindowTop, true, pState);
}

void Toolbar::draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, const ToolbarState& pState) const
{
    Ui::fillRect(pCanvas, mStrip, Ui::kStripBg);
    Ui::line(pCanvas, {static_cast<float>(mStrip.mLeft), mStrip.mBottom + 0.5f},
             {static_cast<float>(mStrip.mRight), mStrip.mBottom + 0.5f}, 1.0f, Ui::kBorder);

    for (int tX : mDividers)
    {
        const float tLineX = static_cast<float>(tX) + 0.5f;
        Ui::line(pCanvas, {tLineX, static_cast<float>(mStrip.mTop - 8)},
                 {tLineX, static_cast<float>(mStrip.mTop - kRowHeight + 8)}, 1.0f, Ui::kBorder);
    }
    for (const Item& tItem : mItems)
    {
        const bool tEnabled = isEnabled(tItem.mAction, pState);
        const bool tHover = tEnabled && tItem.mRect.contains(pMouseX, pMouseY);
        Ui::Button(pCanvas, tItem.mRect, tItem.mLabel, isActive(tItem.mAction, pState), tEnabled, tHover);
    }
}

ToolbarAction Toolbar::hitTest(int pX, int pY, const ToolbarState& pState) const
{
    for (const Item& tItem : mItems)
    {
        if (tItem.mRect.contains(pX, pY) && isEnabled(tItem.mAction, pState))
        {
            return tItem.mAction;
        }
    }
    return ToolbarAction::None;
}

bool Toolbar::contains(int pX, int pY) const
{
    return mStrip.contains(pX, pY);
}

const RectI& Toolbar::strip() const
{
    return mStrip;
}

int Toolbar::height() const
{
    return mStrip.height();
}

RectI Toolbar::buttonRect(ToolbarAction pAction) const
{
    for (const Item& tItem : mItems)
    {
        if (tItem.mAction == pAction)
        {
            return tItem.mRect;
        }
    }
    return RectI();
}

bool Toolbar::isActive(ToolbarAction pAction, const ToolbarState& pState)
{
    switch (pAction)
    {
        case ToolbarAction::Layers:
            return pState.mLayersOpen;
        case ToolbarAction::XMapMode:
            return pState.mModesOpen;
        case ToolbarAction::goToAirport:
            return pState.mSearchOpen;
        case ToolbarAction::FollowAircraft:
            return pState.mFollowing;
        case ToolbarAction::drawTools:
            return pState.mToolsOpen;
        default:
            return false;
    }
}

bool Toolbar::isEnabled(ToolbarAction pAction, const ToolbarState& pState)
{
    switch (pAction)
    {
        case ToolbarAction::FollowAircraft:
        case ToolbarAction::CenterAircraft:
            return pState.mHasAircraft;
        default:
            return true;
    }
}

const char* Toolbar::labelFor(ToolbarAction pAction, const ToolbarState& pState)
{
    switch (pAction)
    {
        case ToolbarAction::Layers:
            return "Layers";
        case ToolbarAction::XMapMode:
            return pState.mModeLabel;
        case ToolbarAction::FollowAircraft:
            return "Follow aircraft";
        case ToolbarAction::CenterAircraft:
            return "Center on aircraft";
        case ToolbarAction::goToAirport:
            return "Go to airport";
        case ToolbarAction::drawTools:
            return "Draw tools";
        case ToolbarAction::popOut:
            return pState.mPoppedOut ? "Pop in" : "Pop out";
        case ToolbarAction::HideControls:
            return "Hide";
        default:
            break;
    }
    return "";
}
