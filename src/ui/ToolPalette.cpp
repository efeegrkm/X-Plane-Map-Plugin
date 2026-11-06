// ToolPalette.cpp
#include "ui/ToolPalette.h"

#include <algorithm>
#include <array>

#include "gfx/Canvas.h"
#include "ui/UiDraw.h"

namespace
{

const int kPadding = 6;        // panel kenari ile icerik arasi
const int kHeaderHeight = 22;  // baslik (tutamak) cubugu
const int kToolHeight = 26;    // arac satiri
const int kButtonHeight = 20;  // islem dugmeleri
const int kGap = 4;
const int kIconBox = 22;       // ikonun kapladigi kare
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

}  // namespace

bool ToolPalette::IsVisible() const
{
    return mVisible;
}

void ToolPalette::SetVisible(bool pVisible)
{
    mVisible = pVisible;
}

bool ToolPalette::IsCollapsed() const
{
    return mCollapsed;
}

void ToolPalette::SetCollapsed(bool pCollapsed)
{
    mCollapsed = pCollapsed;
}

int ToolPalette::OffsetRight() const
{
    return mOffsetRight;
}

int ToolPalette::OffsetTop() const
{
    return mOffsetTop;
}

void ToolPalette::SetOffset(int pRight, int pTop)
{
    mOffsetRight = std::max(pRight, 0);
    mOffsetTop = std::max(pTop, 0);
}

void ToolPalette::MoveBy(int pDeltaX, int pDeltaY)
{
    // Saga kayinca sag kenara uzaklik azalir; yukari kayinca (y artar) ust kenara uzaklik azalir
    SetOffset(mOffsetRight - pDeltaX, mOffsetTop - pDeltaY);
}

ToolbarAction ToolPalette::ActiveTool(const ToolbarState& pState)
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

void ToolPalette::Layout(const RectI& pBounds, const ToolbarState& pState)
{
    mItems.clear();
    // Genislik: islem dugmeleri yan yana sigsin
    int tActionsWidth = 0;
    for (ToolbarAction tAction : kActions)
    {
        tActionsWidth += Ui::ButtonWidth(LabelFor(tAction)) + kGap;
    }
    const int tWidth = std::max(kMinWidth, tActionsWidth - kGap + 2 * kPadding);
    const int tToolCount = mCollapsed ? 1 : static_cast<int>(kTools.size());
    int tHeight = kHeaderHeight + kPadding / 2 + tToolCount * kToolHeight + kPadding / 2;
    if (!mCollapsed)
    {
        tHeight += kPadding + kButtonHeight + kPadding;
    }

    // Yer: sag ust koseye gore; alanin disina tasmasin (tasarsa uzaklik da duzeltilir)
    const int tMaxRight = std::max(pBounds.Width() - tWidth, 0);
    const int tMaxTop = std::max(pBounds.Height() - tHeight, 0);
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
        if (mCollapsed && tAction != ActiveTool(pState))
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
        tItem.mRect.mRight = tX + Ui::ButtonWidth(LabelFor(tAction));
        tItem.mRect.mTop = tY;
        tItem.mRect.mBottom = tY - kButtonHeight;
        mItems.push_back(tItem);
        tX = tItem.mRect.mRight + kGap;
    }
    // Dugmeleri panel genisligine yay: son dugme sag kenara dayansin
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

RectI ToolPalette::ChevronRect() const
{
    RectI tRect = mHeader;
    tRect.mLeft = mHeader.mRight - kChevronWidth - 2;
    tRect.mRight = mHeader.mRight - 2;
    return tRect;
}

bool ToolPalette::Contains(int pX, int pY) const
{
    return mVisible && mRect.Contains(pX, pY);
}

void ToolPalette::Draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, const ToolbarState& pState) const
{
    if (!mVisible)
    {
        return;
    }
    Ui::FillRect(pCanvas, mRect, Ui::kPanelBg);

    // Baslik: tutamak noktalari, ad, katla/ac oku
    Ui::FillRect(pCanvas, mHeader, kHeaderBg);
    const bool tHeaderHover = mHeader.Contains(pMouseX, pMouseY);
    Gfx::Mesh tGrip;
    const float tGripX = static_cast<float>(mHeader.mLeft + 8);
    const float tGripY = static_cast<float>(mHeader.mTop + mHeader.mBottom) * 0.5f;
    for (int tRow = -1; tRow <= 1; ++tRow)
    {
        for (int tColumn = 0; tColumn < 2; ++tColumn)
        {
            const float tX = tGripX + tColumn * 4.0f;
            const float tY = tGripY + tRow * 4.0f;
            tGrip.AddRect({tX, tY + 1.5, tX + 1.5, tY}, tHeaderHover ? Ui::kTextGrey : Ui::kTextDim);
        }
    }
    pCanvas.Draw(tGrip);
    const int tTextY = (mHeader.mTop + mHeader.mBottom) / 2 - Ui::FontHeight() / 2 + 1;
    Ui::Text(mHeader.mLeft + 20, tTextY, kTitle, Ui::kTextGrey);

    const RectI tChevron = ChevronRect();
    const bool tChevronHover = tChevron.Contains(pMouseX, pMouseY);
    {
        const float tX = static_cast<float>(tChevron.mLeft + tChevron.mRight) * 0.5f;
        const float tY = static_cast<float>(tChevron.mTop + tChevron.mBottom) * 0.5f;
        const Rgba& tColor = tChevronHover ? Ui::kTextWhite : Ui::kTextGrey;
        Gfx::Mesh tArrow;
        if (mCollapsed)
        {
            tArrow.AddLine({tX - 4.0f, tY + 2.0f}, {tX, tY - 2.0f}, 1.5f, tColor);  // v: ac
            tArrow.AddLine({tX, tY - 2.0f}, {tX + 4.0f, tY + 2.0f}, 1.5f, tColor);
        }
        else
        {
            tArrow.AddLine({tX - 4.0f, tY - 2.0f}, {tX, tY + 2.0f}, 1.5f, tColor);  // ^: katla
            tArrow.AddLine({tX, tY + 2.0f}, {tX + 4.0f, tY - 2.0f}, 1.5f, tColor);
        }
        pCanvas.Draw(tArrow);
    }
    Ui::Line(pCanvas, {static_cast<float>(mHeader.mLeft), mHeader.mBottom + 0.5f},
             {static_cast<float>(mHeader.mRight), mHeader.mBottom + 0.5f}, 1.0f, Ui::kBorder);

    for (const Item& tItem : mItems)
    {
        const bool tEnabled = IsEnabled(tItem.mAction, pState);
        const bool tHover = tEnabled && tItem.mRect.Contains(pMouseX, pMouseY);
        const bool tActive = IsActive(tItem.mAction, pState);
        if (!tItem.mIsTool)
        {
            Ui::Button(pCanvas, tItem.mRect, LabelFor(tItem.mAction), false, tEnabled, tHover);
            continue;
        }
        // Arac satiri: secili ise mavi zemin + solda vurgu cubugu
        if (tActive)
        {
            Ui::FillRect(pCanvas, tItem.mRect, kToolActiveBg);
            RectI tAccent = tItem.mRect;
            tAccent.mRight = tAccent.mLeft + 3;
            Ui::FillRect(pCanvas, tAccent, Ui::kButtonActiveBorder);
        }
        else if (tHover)
        {
            Ui::FillRect(pCanvas, tItem.mRect, kHoverBg);
        }
        const Rgba& tColor = tActive ? Ui::kTextWhite : Ui::kTextGrey;
        const float tIconX = static_cast<float>(tItem.mRect.mLeft + 6 + kIconBox / 2);
        const float tIconY = static_cast<float>(tItem.mRect.mTop + tItem.mRect.mBottom) * 0.5f;
        DrawIcon(pCanvas, tItem.mAction, {tIconX, tIconY}, tColor);
        const int tLabelY = (tItem.mRect.mTop + tItem.mRect.mBottom) / 2 - Ui::FontHeight() / 2 + 1;
        Ui::Text(tItem.mRect.mLeft + 6 + kIconBox + 8, tLabelY, LabelFor(tItem.mAction),
                 tActive || tHover ? Ui::kTextWhite : Ui::kTextGrey);
    }

    if (mSeparatorY != 0)
    {
        Ui::Line(pCanvas, {static_cast<float>(mRect.mLeft + kPadding), mSeparatorY + 0.5f},
                 {static_cast<float>(mRect.mRight - kPadding), mSeparatorY + 0.5f}, 1.0f, Ui::kBorder);
    }
    Ui::FrameRect(pCanvas, mRect, Ui::kBorder);
}

void ToolPalette::DrawIcon(Gfx::Canvas& pCanvas, ToolbarAction pAction, const PointF& pCenter, const Rgba& pColor)
{
    // Sade, tek renkli vektor ikonlar (~14 boxel)
    const float tX = pCenter.mX;
    const float tY = pCenter.mY;
    Gfx::Mesh tMesh;
    switch (pAction)
    {
        case ToolbarAction::ModePan:
        {
            // Dort yonlu ok
            tMesh.AddLine({tX - 5.0f, tY}, {tX + 5.0f, tY}, 1.5f, pColor);
            tMesh.AddLine({tX, tY - 5.0f}, {tX, tY + 5.0f}, 1.5f, pColor);
            tMesh.AddTriangle({tX + 7.0f, tY}, {tX + 4.0f, tY + 3.0f}, {tX + 4.0f, tY - 3.0f}, pColor);
            tMesh.AddTriangle({tX - 7.0f, tY}, {tX - 4.0f, tY - 3.0f}, {tX - 4.0f, tY + 3.0f}, pColor);
            tMesh.AddTriangle({tX, tY + 7.0f}, {tX - 3.0f, tY + 4.0f}, {tX + 3.0f, tY + 4.0f}, pColor);
            tMesh.AddTriangle({tX, tY - 7.0f}, {tX + 3.0f, tY - 4.0f}, {tX - 3.0f, tY - 4.0f}, pColor);
            break;
        }
        case ToolbarAction::ModePoint:
        {
            // Harita ignesi: halka + asagi sivri uc + ortada nokta
            tMesh.AddRing({tX, tY + 2.0f}, 4.5f, 1.6f, pColor);
            tMesh.AddTriangle({tX - 3.6f, tY - 0.6f}, {tX + 3.6f, tY - 0.6f}, {tX, tY - 7.0f}, pColor);
            tMesh.AddDisc({tX, tY + 2.0f}, 1.5f, pColor);
            break;
        }
        case ToolbarAction::ModeRoute:
        {
            // Kirik cizgi + dugumler
            const std::vector<Gfx::Point> tPoints = {
                {tX - 6.5f, tY - 4.5f}, {tX - 2.0f, tY + 3.5f}, {tX + 2.0f, tY - 2.0f}, {tX + 6.5f, tY + 5.0f}};
            tMesh.AddPolyline(tPoints, false, 1.5f, pColor);
            for (const Gfx::Point& tPoint : tPoints)
            {
                tMesh.AddDisc(tPoint, 2.0f, pColor);
            }
            break;
        }
        case ToolbarAction::ModeRegion:
        {
            // Yari saydam dolgulu besgen
            tMesh.AddRegularPolygon({tX, tY}, 6.5f, 5, 90.0f, 1.5f, pColor);
            break;
        }
        default:
            return;
    }
    if (pAction == ToolbarAction::ModeRegion)
    {
        Gfx::Mesh tFill;
        tFill.AddDisc({tX, tY - 0.5f}, 4.5f, WithAlpha(pColor, 0.30f));
        pCanvas.Draw(tFill);
    }
    pCanvas.Draw(tMesh);
}

ToolPalette::Hit ToolPalette::HitTest(int pX, int pY, const ToolbarState& pState) const
{
    Hit tHit;
    if (!Contains(pX, pY))
    {
        return tHit;
    }
    tHit.mInside = true;
    if (mHeader.Contains(pX, pY))
    {
        if (ChevronRect().Contains(pX, pY))
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
        if (!tItem.mRect.Contains(pX, pY) || !IsEnabled(tItem.mAction, pState))
        {
            continue;
        }
        if (mCollapsed && tItem.mIsTool)
        {
            tHit.mToggleCollapse = true;  // katliyken secili araca tiklamak paneli acar
        }
        else
        {
            tHit.mAction = tItem.mAction;
        }
        return tHit;
    }
    return tHit;
}

const char* ToolPalette::LabelFor(ToolbarAction pAction)
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

bool ToolPalette::IsActive(ToolbarAction pAction, const ToolbarState& pState)
{
    return pAction == ActiveTool(pState);
}

bool ToolPalette::IsEnabled(ToolbarAction pAction, const ToolbarState& pState)
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
