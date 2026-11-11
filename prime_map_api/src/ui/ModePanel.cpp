// ModePanel.cpp
#include "ui/ModePanel.h"

#include <algorithm>

#include "ui/UiDraw.h"

namespace
{

const int kPadding = 8;
const int kTitleHeight = 20;
const int kRowHeight = 22;
const int kBoxSize = 14;
const int kMinWidth = 130;

}  // namespace

bool ModePanel::IsOpen() const
{
    return mOpen;
}

void ModePanel::SetOpen(bool pOpen)
{
    mOpen = pOpen;
}

void ModePanel::Toggle()
{
    mOpen = !mOpen;
}

void ModePanel::Layout(const RectI& pAnchor)
{
    int tWidth = kMinWidth;
    for (int tIndex = 0; tIndex < kMapModeCount; ++tIndex)
    {
        const int tRowWidth =
            kPadding + kBoxSize + 8 + Ui::TextWidth(ThemeFor(static_cast<MapMode>(tIndex)).mName) + kPadding;
        tWidth = std::max(tWidth, tRowWidth);
    }
    mRect.mLeft = pAnchor.mLeft;
    mRect.mRight = pAnchor.mLeft + tWidth;
    mRect.mTop = pAnchor.mBottom - 4;
    mRect.mBottom = mRect.mTop - (kPadding + kTitleHeight + kMapModeCount * kRowHeight + kPadding);
}

RectI ModePanel::RowRect(int pIndex) const
{
    RectI tRow;
    tRow.mLeft = mRect.mLeft + kPadding;
    tRow.mRight = mRect.mRight - kPadding;
    tRow.mTop = mRect.mTop - kPadding - kTitleHeight - pIndex * kRowHeight;
    tRow.mBottom = tRow.mTop - kRowHeight;
    return tRow;
}

void ModePanel::Draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, MapMode pCurrent) const
{
    if (!mOpen)
    {
        return;
    }
    Ui::FillRect(pCanvas, mRect, Ui::kPanelBg);
    Ui::FrameRect(pCanvas, mRect, Ui::kBorder);
    Ui::Text(mRect.mLeft + kPadding, mRect.mTop - kPadding - 12, "MAP MODE", Ui::kTextGrey);
    for (int tIndex = 0; tIndex < kMapModeCount; ++tIndex)
    {
        const MapMode tMode = static_cast<MapMode>(tIndex);
        const RectI tRow = RowRect(tIndex);
        RectI tBox;
        tBox.mLeft = tRow.mLeft;
        tBox.mRight = tBox.mLeft + kBoxSize;
        tBox.mTop = (tRow.mTop + tRow.mBottom) / 2 + kBoxSize / 2;
        tBox.mBottom = tBox.mTop - kBoxSize;
        const bool tSelected = tMode == pCurrent;
        Ui::CheckBox(pCanvas, tBox, tSelected, tRow.Contains(pMouseX, pMouseY));
        const int tTextY = (tRow.mTop + tRow.mBottom) / 2 - Ui::FontHeight() / 2 + 1;
        Ui::Text(tBox.mRight + 8, tTextY, ThemeFor(tMode).mName, tSelected ? Ui::kTextWhite : Ui::kTextGrey);
    }
}

bool ModePanel::HitTest(int pX, int pY, std::optional<MapMode>* pOutMode) const
{
    pOutMode->reset();
    if (!mOpen || !mRect.Contains(pX, pY))
    {
        return false;
    }
    for (int tIndex = 0; tIndex < kMapModeCount; ++tIndex)
    {
        if (RowRect(tIndex).Contains(pX, pY))
        {
            *pOutMode = static_cast<MapMode>(tIndex);
        }
    }
    return true;
}
