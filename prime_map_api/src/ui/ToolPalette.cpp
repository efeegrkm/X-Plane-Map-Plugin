#include "ui/ToolPalette.h"

#include <algorithm>
#include <array>

#include "gfx/Canvas.h"
#include "ui/UiDraw.h"

namespace
{
const int kPadding = 6;
const int kHeaderHeight = 22;
const int kToolHeight = 26;
const int kButtonHeight = 20;
const int kGap = 4;
const int kIconBox = 22;
const int kChevronWidth = 18;
const int kMinWidth = 150;
const char* const kTitle = "DRAW";

const auto kTools = std::to_array<ToolbarAction>(
    {ToolbarAction::ModePan, ToolbarAction::ModePoint, ToolbarAction::ModeRoute, ToolbarAction::ModeRegion});
const auto kActions = std::to_array<ToolbarAction>(
    {ToolbarAction::Finish, ToolbarAction::Undo, ToolbarAction::Clear});

const Rgba kHeaderBg = {0.12f, 0.13f, 0.16f, 0.95f};
const Rgba kToolActiveBg = {0.00f, 0.48f, 0.70f, 0.55f};
const Rgba kHoverBg = {1.0f, 1.0f, 1.0f, 0.07f};
}

bool ToolPalette::isVisible() const
{
    return mVisible;
}

void ToolPalette::setVisible(bool pVisible)
{
    mVisible = pVisible;
}

bool ToolPalette::isCollapsed() const
{
    return mCollapsed;
}

void ToolPalette::setCollapsed(bool pCollapsed)
{
    mCollapsed = pCollapsed;
}

int ToolPalette::offsetRight() const
{
    return mOffsetRight;
}

int ToolPalette::offsetTop() const
{
    return mOffsetTop;
}

void ToolPalette::setOffset(int pRight, int pTop)
{
    mOffsetRight = std::max(pRight, 0);
    mOffsetTop = std::max(pTop, 0);
}

void ToolPalette::moveBy(int pDeltaX, int pDeltaY)
{
    setOffset(mOffsetRight - pDeltaX, mOffsetTop - pDeltaY);
}

ToolbarAction ToolPalette::activeTool(const ToolbarState& pState)
{
    switch (pState.mMode)
    {
        case Drawings::Mode::Point:
            return ToolbarAction::ModePoint;
        case Drawings::Mode::Route:
            return ToolbarAction::ModeRoute;
        case Drawings::Mode::Region:
            return ToolbarAction::ModeRegion;
        case Drawings::Mode::Pan:
            break;
    }
    return ToolbarAction::ModePan;
}

void ToolPalette::layout(const RectI& pBounds, const ToolbarState& pState)
{
    mItems.clear();
    int tActionsWidth = 0;
    for (ToolbarAction tAction : kActions)
    {
        tActionsWidth += Ui::buttonWidth(labelFor(tAction)) + kGap;
    }
    const int tWidth = std::max(kMinWidth, tActionsWidth - kGap + 2 * kPadding);
    const int tToolCount = mCollapsed ? 1 : static_cast<int>(kTools.size());
    int tHeight = kHeaderHeight + kPadding / 2 + tToolCount * kToolHeight + kPadding / 2;
    if (!mCollapsed)
    {
        tHeight += kPadding + kButtonHeight + kPadding;
    }

    const int tMaxRight = std::max(pBounds.width() - tWidth, 0);
    const int tMaxTop = std::max(pBounds.height() - tHeight, 0);
    mOffsetRight = std::clamp(mOffsetRight, 0, tMaxRight);
    mOffsetTop = std::clamp(mOffsetTop, 0, tMaxTop);
    mRect.mRight = pBounds.mRight - mOffsetRight;
    mRect.mLeft = mRect.mRight - tWidth;
    mRect.mTop = pBounds.mTop - mOffsetTop;
    mRect.mBottom = mRect.mTop - tHeight;

    mHeader = mRect;
    mHeader.mBottom = mRect.mTop - kHeaderHeight;

    int tY = mHeader.mBottom - kPadding / 2;
    for (ToolbarAction tAction : kTools)
    {
        if (mCollapsed && tAction != activeTool(pState))
        {
            continue;
        }
        Item tItem;
        tItem.mAction = tAction;
        tItem.mIsTool = true;
        tItem.mRect.mLeft = mRect.mLeft + kPadding / 2;
        tItem.mRect.mRight = mRect.mRight - kPadding / 2;
        tItem.mRect.mTop = tY;
        tItem.mRect.mBottom = tY - kToolHeight;
        mItems.push_back(tItem);
        tY -= kToolHeight;
    }
    mSeparatorY = 0;
    if (mCollapsed)
    {
        return;
    }
    tY -= kPadding / 2;
    mSeparatorY = tY;
    tY -= kPadding;
    int tX = mRect.mLeft + kPadding;
    for (ToolbarAction tAction : kActions)
    {
        Item tItem;
        tItem.mAction = tAction;
        tItem.mIsTool = false;
        tItem.mRect.mLeft = tX;
        tItem.mRect.mRight = tX + Ui::buttonWidth(labelFor(tAction));
        tItem.mRect.mTop = tY;
        tItem.mRect.mBottom = tY - kButtonHeight;
        mItems.push_back(tItem);
        tX = tItem.mRect.mRight + kGap;
    }
    const int tSpare = (mRect.mRight - kPadding) - (tX - kGap);
    if (tSpare > 0)
    {
        const int tCount = static_cast<int>(kActions.size());
        int tShift = 0;
        for (Item& tItem : mItems)
        {
            if (tItem.mIsTool)
            {
                continue;
            }
            const int tExtra = tSpare / tCount;
            tItem.mRect.mLeft += tShift;
            tShift += tExtra;
            tItem.mRect.mRight += tShift;
        }
    }
}

RectI ToolPalette::chevronRect() const
{
    RectI tRect = mHeader;
    tRect.mLeft = mHeader.mRight - kChevronWidth - 2;
    tRect.mRight = mHeader.mRight - 2;
    return tRect;
}

bool ToolPalette::contains(int pX, int pY) const
{
    return mVisible && mRect.contains(pX, pY);
}

void ToolPalette::draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, const ToolbarState& pState) const
{
    if (!mVisible)
    {
        return;
    }
    Ui::fillRect(pCanvas, mRect, Ui::kPanelBg);

    Ui::fillRect(pCanvas, mHeader, kHeaderBg);
    const bool tHeaderHover = mHeader.contains(pMouseX, pMouseY);
    Gfx::Mesh tGrip;
    const float tGripX = static_cast<float>(mHeader.mLeft + 8);
    const float tGripY = static_cast<float>(mHeader.mTop + mHeader.mBottom) * 0.5f;
    for (int tRow = -1; tRow <= 1; ++tRow)
    {
        for (int tColumn = 0; tColumn < 2; ++tColumn)
        {
            const float tX = tGripX + tColumn * 4.0f;
            const float tY = tGripY + tRow * 4.0f;
            tGrip.addRect({tX, tY + 1.5, tX + 1.5, tY}, tHeaderHover ? Ui::kTextGrey : Ui::kTextDim);
        }
    }
    pCanvas.draw(tGrip);
    const int tTextY = (mHeader.mTop + mHeader.mBottom) / 2 - Ui::fontHeight() / 2 + 1;
    Ui::text(mHeader.mLeft + 20, tTextY, kTitle, Ui::kTextGrey);

    const RectI tChevron = chevronRect();
    const bool tChevronHover = tChevron.contains(pMouseX, pMouseY);
    {
        const float tX = static_cast<float>(tChevron.mLeft + tChevron.mRight) * 0.5f;
        const float tY = static_cast<float>(tChevron.mTop + tChevron.mBottom) * 0.5f;
        const Rgba& tColor = tChevronHover ? Ui::kTextWhite : Ui::kTextGrey;
        Gfx::Mesh tArrow;
        if (mCollapsed)
        {
            tArrow.addLine({tX - 4.0f, tY + 2.0f}, {tX, tY - 2.0f}, 1.5f, tColor);
            tArrow.addLine({tX, tY - 2.0f}, {tX + 4.0f, tY + 2.0f}, 1.5f, tColor);
        }
        else
        {
            tArrow.addLine({tX - 4.0f, tY - 2.0f}, {tX, tY + 2.0f}, 1.5f, tColor);
            tArrow.addLine({tX, tY + 2.0f}, {tX + 4.0f, tY - 2.0f}, 1.5f, tColor);
        }
        pCanvas.draw(tArrow);
    }
    Ui::line(pCanvas, {static_cast<float>(mHeader.mLeft), mHeader.mBottom + 0.5f},
             {static_cast<float>(mHeader.mRight), mHeader.mBottom + 0.5f}, 1.0f, Ui::kBorder);

    for (const Item& tItem : mItems)
    {
        const bool tEnabled = isEnabled(tItem.mAction, pState);
        const bool tHover = tEnabled && tItem.mRect.contains(pMouseX, pMouseY);
        const bool tActive = isActive(tItem.mAction, pState);
        if (!tItem.mIsTool)
        {
            Ui::Button(pCanvas, tItem.mRect, labelFor(tItem.mAction), false, tEnabled, tHover);
            continue;
        }
        if (tActive)
        {
            Ui::fillRect(pCanvas, tItem.mRect, kToolActiveBg);
            RectI tAccent = tItem.mRect;
            tAccent.mRight = tAccent.mLeft + 3;
            Ui::fillRect(pCanvas, tAccent, Ui::kButtonActiveBorder);
        }
        else if (tHover)
        {
            Ui::fillRect(pCanvas, tItem.mRect, kHoverBg);
        }
        const Rgba& tColor = tActive ? Ui::kTextWhite : Ui::kTextGrey;
        const float tIconX = static_cast<float>(tItem.mRect.mLeft + 6 + kIconBox / 2);
        const float tIconY = static_cast<float>(tItem.mRect.mTop + tItem.mRect.mBottom) * 0.5f;
        drawIcon(pCanvas, tItem.mAction, {tIconX, tIconY}, tColor);
        const int tLabelY = (tItem.mRect.mTop + tItem.mRect.mBottom) / 2 - Ui::fontHeight() / 2 + 1;
        Ui::text(tItem.mRect.mLeft + 6 + kIconBox + 8, tLabelY, labelFor(tItem.mAction),
                 tActive || tHover ? Ui::kTextWhite : Ui::kTextGrey);
    }

    if (mSeparatorY != 0)
    {
        Ui::line(pCanvas, {static_cast<float>(mRect.mLeft + kPadding), mSeparatorY + 0.5f},
                 {static_cast<float>(mRect.mRight - kPadding), mSeparatorY + 0.5f}, 1.0f, Ui::kBorder);
    }
    Ui::frameRect(pCanvas, mRect, Ui::kBorder);
}

void ToolPalette::drawIcon(Gfx::Canvas& pCanvas, ToolbarAction pAction, const PointF& pCenter, const Rgba& pColor)
{
    const float tX = pCenter.mX;
    const float tY = pCenter.mY;
    Gfx::Mesh tMesh;
    switch (pAction)
    {
        case ToolbarAction::ModePan:
        {
            tMesh.addLine({tX - 5.0f, tY}, {tX + 5.0f, tY}, 1.5f, pColor);
            tMesh.addLine({tX, tY - 5.0f}, {tX, tY + 5.0f}, 1.5f, pColor);
            tMesh.addTriangle({tX + 7.0f, tY}, {tX + 4.0f, tY + 3.0f}, {tX + 4.0f, tY - 3.0f}, pColor);
            tMesh.addTriangle({tX - 7.0f, tY}, {tX - 4.0f, tY - 3.0f}, {tX - 4.0f, tY + 3.0f}, pColor);
            tMesh.addTriangle({tX, tY + 7.0f}, {tX - 3.0f, tY + 4.0f}, {tX + 3.0f, tY + 4.0f}, pColor);
            tMesh.addTriangle({tX, tY - 7.0f}, {tX + 3.0f, tY - 4.0f}, {tX - 3.0f, tY - 4.0f}, pColor);
            break;
        }
        case ToolbarAction::ModePoint:
        {
            tMesh.addRing({tX, tY + 2.0f}, 4.5f, 1.6f, pColor);
            tMesh.addTriangle({tX - 3.6f, tY - 0.6f}, {tX + 3.6f, tY - 0.6f}, {tX, tY - 7.0f}, pColor);
            tMesh.addDisc({tX, tY + 2.0f}, 1.5f, pColor);
            break;
        }
        case ToolbarAction::ModeRoute:
        {
            const std::vector<Gfx::Point> tPoints = {
                {tX - 6.5f, tY - 4.5f}, {tX - 2.0f, tY + 3.5f}, {tX + 2.0f, tY - 2.0f}, {tX + 6.5f, tY + 5.0f}};
            tMesh.addPolyline(tPoints, false, 1.5f, pColor);
            for (const Gfx::Point& tPoint : tPoints)
            {
                tMesh.addDisc(tPoint, 2.0f, pColor);
            }
            break;
        }
        case ToolbarAction::ModeRegion:
        {
            tMesh.addRegularPolygon({tX, tY}, 6.5f, 5, 90.0f, 1.5f, pColor);
            break;
        }
        default:
            return;
    }
    if (pAction == ToolbarAction::ModeRegion)
    {
        Gfx::Mesh tFill;
        tFill.addDisc({tX, tY - 0.5f}, 4.5f, withAlpha(pColor, 0.30f));
        pCanvas.draw(tFill);
    }
    pCanvas.draw(tMesh);
}

ToolPalette::Hit ToolPalette::hitTest(int pX, int pY, const ToolbarState& pState) const
{
    Hit tHit;
    if (!contains(pX, pY))
    {
        return tHit;
    }
    tHit.mInside = true;
    if (mHeader.contains(pX, pY))
    {
        if (chevronRect().contains(pX, pY))
        {
            tHit.mToggleCollapse = true;
        }
        else
        {
            tHit.mStartDrag = true;
        }
        return tHit;
    }
    for (const Item& tItem : mItems)
    {
        if (!tItem.mRect.contains(pX, pY) || !isEnabled(tItem.mAction, pState))
        {
            continue;
        }
        if (mCollapsed && tItem.mIsTool)
        {
            tHit.mToggleCollapse = true;
        }
        else
        {
            tHit.mAction = tItem.mAction;
        }
        return tHit;
    }
    return tHit;
}

const char* ToolPalette::labelFor(ToolbarAction pAction)
{
    switch (pAction)
    {
        case ToolbarAction::ModePan:
            return "Pan";
        case ToolbarAction::ModePoint:
            return "Point";
        case ToolbarAction::ModeRoute:
            return "Route";
        case ToolbarAction::ModeRegion:
            return "Region";
        case ToolbarAction::Finish:
            return "Finish";
        case ToolbarAction::Undo:
            return "Undo";
        case ToolbarAction::Clear:
            return "Clear";
        default:
            return "";
    }
}

bool ToolPalette::isActive(ToolbarAction pAction, const ToolbarState& pState)
{
    return pAction == activeTool(pState);
}

bool ToolPalette::isEnabled(ToolbarAction pAction, const ToolbarState& pState)
{
    switch (pAction)
    {
        case ToolbarAction::Finish:
            return pState.mCanFinish;
        case ToolbarAction::Undo:
        case ToolbarAction::Clear:
            return pState.mCanUndo;
        default:
            return true;
    }
}
