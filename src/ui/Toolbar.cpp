// Toolbar.cpp
#include "ui/Toolbar.h"

#include "ui/UiDraw.h"

namespace
{

const int kMargin = 4;  // serit kenari ile dugme arasi
const int kGap = 4;     // iki dugme arasi

}  // namespace

void Toolbar::Layout(const RectI& pArea, const ToolbarState& pState)
{
    mStrip.mLeft = pArea.mLeft;
    mStrip.mRight = pArea.mRight;
    mStrip.mTop = pArea.mTop;
    mStrip.mBottom = pArea.mTop - kHeight;

    mItems.clear();
    const int tTop = mStrip.mTop - kMargin;
    const int tBottom = mStrip.mBottom + kMargin;

    // Soldan saga dizilen dugmeler
    const ToolbarAction tLeftActions[] = {ToolbarAction::Layers, ToolbarAction::FollowAircraft,
                                          ToolbarAction::CenterAircraft};
    int tX = mStrip.mLeft + kMargin;
    for (ToolbarAction tAction : tLeftActions)
    {
        const char* tLabel = LabelFor(tAction, pState);
        Item tItem;
        tItem.mAction = tAction;
        tItem.mLabel = tLabel;
        tItem.mRect.mLeft = tX;
        tItem.mRect.mRight = tX + Ui::ButtonWidth(tLabel);
        tItem.mRect.mTop = tTop;
        tItem.mRect.mBottom = tBottom;
        mItems.push_back(tItem);
        tX = tItem.mRect.mRight + kGap;
    }

    // Sagdan sola dizilen dugmeler
    const ToolbarAction tRightActions[] = {ToolbarAction::HideControls, ToolbarAction::PopOut};
    tX = mStrip.mRight - kMargin;
    for (ToolbarAction tAction : tRightActions)
    {
        const char* tLabel = LabelFor(tAction, pState);
        Item tItem;
        tItem.mAction = tAction;
        tItem.mLabel = tLabel;
        tItem.mRect.mRight = tX;
        tItem.mRect.mLeft = tX - Ui::ButtonWidth(tLabel);
        tItem.mRect.mTop = tTop;
        tItem.mRect.mBottom = tBottom;
        mItems.push_back(tItem);
        tX = tItem.mRect.mLeft - kGap;
    }
}

void Toolbar::Draw(int pMouseX, int pMouseY, const ToolbarState& pState) const
{
    Ui::FillRect(mStrip, Ui::kStripBg);
    Ui::Line(static_cast<float>(mStrip.mLeft), mStrip.mBottom + 0.5f, static_cast<float>(mStrip.mRight),
             mStrip.mBottom + 0.5f, 1.0f, Ui::kBorder);

    for (const Item& tItem : mItems)
    {
        const bool tEnabled = IsEnabled(tItem.mAction, pState);
        const bool tHover = tEnabled && tItem.mRect.Contains(pMouseX, pMouseY);
        Ui::Button(tItem.mRect, tItem.mLabel, IsActive(tItem.mAction, pState), tEnabled, tHover);
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
        case ToolbarAction::FollowAircraft:
            return pState.mFollowing;
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
            return "Katmanlar";
        case ToolbarAction::FollowAircraft:
            return "Ucagi takip et";
        case ToolbarAction::CenterAircraft:
            return "Ucaga ortala";
        case ToolbarAction::PopOut:
            return pState.mPoppedOut ? "Iceri al" : "Disari al";
        case ToolbarAction::HideControls:
            return "Gizle";
        case ToolbarAction::None:
            break;
    }
    return "";
}
