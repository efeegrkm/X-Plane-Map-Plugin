// Toolbar.cpp
#include "ui/Toolbar.h"

#include "ui/UiDraw.h"

namespace
{

const int kMargin = 4;      // serit kenari ile dugme arasi
const int kGap = 4;         // iki dugme arasi
const int kGroupGap = 14;   // dugme gruplari arasi

// Dugme gruplari (soldan saga). ToolbarAction::None = gruplar arasi ayirici.
const std::vector<ToolbarAction> kGeneralGroup = {
    ToolbarAction::Layers,         ToolbarAction::MapMode, ToolbarAction::None, ToolbarAction::FollowAircraft,
    ToolbarAction::CenterAircraft, ToolbarAction::GoToAirport, ToolbarAction::None, ToolbarAction::DrawTools};
const std::vector<ToolbarAction> kWindowGroup = {ToolbarAction::HideControls, ToolbarAction::PopOut};  // sagdan sola

}  // namespace

int Toolbar::RowWidth(const std::vector<ToolbarAction>& pActions, const ToolbarState& pState)
{
    int tWidth = 0;
    for (ToolbarAction tAction : pActions)
    {
        tWidth += (tAction == ToolbarAction::None) ? kGroupGap : Ui::ButtonWidth(LabelFor(tAction, pState)) + kGap;
    }
    return tWidth;
}

int Toolbar::PlaceRow(const std::vector<ToolbarAction>& pActions, int pX, int pTop, bool pRightToLeft,
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
        const char* tLabel = LabelFor(tAction, pState);
        const int tWidth = Ui::ButtonWidth(tLabel);
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

void Toolbar::Layout(const RectI& pArea, const ToolbarState& pState)
{
    mItems.clear();
    mDividers.clear();
    const int tAvailable = pArea.Width() - 2 * kMargin;
    const int tNeeded = RowWidth(kGeneralGroup, pState) + kGroupGap + RowWidth(kWindowGroup, pState);
    const bool tTwoRows = tNeeded > tAvailable;

    mStrip.mLeft = pArea.mLeft;
    mStrip.mRight = pArea.mRight;
    mStrip.mTop = pArea.mTop;
    mStrip.mBottom = pArea.mTop - (tTwoRows ? 2 * kRowHeight - kMargin : kRowHeight);

    PlaceRow(kGeneralGroup, mStrip.mLeft + kMargin, mStrip.mTop, false, pState);
    const int tWindowTop = tTwoRows ? mStrip.mTop - kRowHeight + kMargin : mStrip.mTop;
    PlaceRow(kWindowGroup, mStrip.mRight - kMargin, tWindowTop, true, pState);
}

void Toolbar::Draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, const ToolbarState& pState) const
{
    Ui::FillRect(pCanvas, mStrip, Ui::kStripBg);
    Ui::Line(pCanvas, {static_cast<float>(mStrip.mLeft), mStrip.mBottom + 0.5f},
             {static_cast<float>(mStrip.mRight), mStrip.mBottom + 0.5f}, 1.0f, Ui::kBorder);

    for (int tX : mDividers)
    {
        const float tLineX = static_cast<float>(tX) + 0.5f;
        Ui::Line(pCanvas, {tLineX, static_cast<float>(mStrip.mTop - 8)},
                 {tLineX, static_cast<float>(mStrip.mTop - kRowHeight + 8)}, 1.0f, Ui::kBorder);
    }
    for (const Item& tItem : mItems)
    {
        const bool tEnabled = IsEnabled(tItem.mAction, pState);
        const bool tHover = tEnabled && tItem.mRect.Contains(pMouseX, pMouseY);
        Ui::Button(pCanvas, tItem.mRect, tItem.mLabel, IsActive(tItem.mAction, pState), tEnabled, tHover);
    }
}

ToolbarAction Toolbar::HitTest(int pX, int pY, const ToolbarState& pState) const
{
    for (const Item& tItem : mItems)
    {
        if (tItem.mRect.Contains(pX, pY) && IsEnabled(tItem.mAction, pState))
        {
            return tItem.mAction;
        }
    }
    return ToolbarAction::None;
}

bool Toolbar::Contains(int pX, int pY) const
{
    return mStrip.Contains(pX, pY);
}

const RectI& Toolbar::Strip() const
{
    return mStrip;
}

int Toolbar::Height() const
{
    return mStrip.Height();
}

RectI Toolbar::ButtonRect(ToolbarAction pAction) const
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

bool Toolbar::IsActive(ToolbarAction pAction, const ToolbarState& pState)
{
    switch (pAction)
    {
        case ToolbarAction::Layers:
            return pState.mLayersOpen;
        case ToolbarAction::MapMode:
            return pState.mModesOpen;
        case ToolbarAction::GoToAirport:
            return pState.mSearchOpen;
        case ToolbarAction::FollowAircraft:
            return pState.mFollowing;
        case ToolbarAction::DrawTools:
            return pState.mToolsOpen;
        default:
            return false;
    }
}

bool Toolbar::IsEnabled(ToolbarAction pAction, const ToolbarState& pState)
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

const char* Toolbar::LabelFor(ToolbarAction pAction, const ToolbarState& pState)
{
    switch (pAction)
    {
        case ToolbarAction::Layers:
            return "Layers";
        case ToolbarAction::MapMode:
            return pState.mModeLabel;
        case ToolbarAction::FollowAircraft:
            return "Follow aircraft";
        case ToolbarAction::CenterAircraft:
            return "Center on aircraft";
        case ToolbarAction::GoToAirport:
            return "Go to airport";
        case ToolbarAction::DrawTools:
            return "Draw tools";
        case ToolbarAction::PopOut:
            return pState.mPoppedOut ? "Pop in" : "Pop out";
        case ToolbarAction::HideControls:
            return "Hide";
        default:
            break;
    }
    return "";
}
