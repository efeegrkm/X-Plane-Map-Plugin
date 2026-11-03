// LayerPanel.cpp
#include "ui/LayerPanel.h"

#include "LayerManager.h"
#include "ui/UiDraw.h"

namespace
{

const int kPadding = 8;       // panel kenari ile icerik arasi
const int kTitleHeight = 20;  // "KATMANLAR" basligi
const int kRowHeight = 22;    // her katman satiri
const int kBoxSize = 14;      // onay kutusu
const int kMinWidth = 150;

}  // namespace

bool LayerPanel::IsOpen() const
{
    return mOpen;
}

void LayerPanel::SetOpen(bool pOpen)
{
    mOpen = pOpen;
}

void LayerPanel::Toggle()
{
    mOpen = !mOpen;
}

void LayerPanel::Layout(const RectI& pAnchor, const LayerManager& pLayers)
{
    mRowCount = pLayers.Count();

    // Genislik: en uzun katman adi + onay kutusu
    int tWidth = kMinWidth;
    for (int tIndex = 0; tIndex < mRowCount; ++tIndex)
    {
        const char* tName = pLayers.At(tIndex)->Name().c_str();
        const int tRowWidth = kPadding + kBoxSize + 8 + Ui::TextWidth(tName) + kPadding;
        if (tRowWidth > tWidth)
        {
            tWidth = tRowWidth;
        }
    }

    mRect.mLeft = pAnchor.mLeft;
    mRect.mRight = pAnchor.mLeft + tWidth;
    mRect.mTop = pAnchor.mBottom - 4;
    mRect.mBottom = mRect.mTop - (kPadding + kTitleHeight + mRowCount * kRowHeight + kPadding);
}

void LayerPanel::Draw(int pMouseX, int pMouseY, const LayerManager& pLayers) const
{
    if (!mOpen)
    {
        return;
    }
    Ui::FillRect(mRect, Ui::kPanelBg);
    Ui::FrameRect(mRect, Ui::kBorder);
    Ui::Text(mRect.mLeft + kPadding, mRect.mTop - kPadding - 12, "KATMANLAR", Ui::kTextGrey);

    for (int tIndex = 0; tIndex < mRowCount && tIndex < pLayers.Count(); ++tIndex)
    {
        const Layer* tLayer = pLayers.At(tIndex);
        const RectI tRow = RowRect(tIndex);
        const bool tHover = tRow.Contains(pMouseX, pMouseY);

        RectI tBox;
        tBox.mLeft = tRow.mLeft;
        tBox.mRight = tRow.mLeft + kBoxSize;
        tBox.mTop = (tRow.mTop + tRow.mBottom) / 2 + kBoxSize / 2;
        tBox.mBottom = tBox.mTop - kBoxSize;
        Ui::CheckBox(tBox, tLayer->IsVisible(), tHover);

        const int tTextY = (tRow.mTop + tRow.mBottom) / 2 - Ui::FontHeight() / 2 + 1;
        Ui::Text(tBox.mRight + 8, tTextY, tLayer->Name().c_str(),
                 tLayer->IsVisible() ? Ui::kTextWhite : Ui::kTextGrey);
    }
}

bool LayerPanel::HitTest(int pX, int pY, int* pOutIndex) const
{
    *pOutIndex = -1;
    if (!mOpen || !mRect.Contains(pX, pY))
    {
        return false;
    }
    for (int tIndex = 0; tIndex < mRowCount; ++tIndex)
    {
        if (RowRect(tIndex).Contains(pX, pY))
        {
            *pOutIndex = tIndex;
            break;
        }
    }
    return true;
}

RectI LayerPanel::RowRect(int pIndex) const
{
    RectI tRow;
    tRow.mLeft = mRect.mLeft + kPadding;
    tRow.mRight = mRect.mRight - kPadding;
    tRow.mTop = mRect.mTop - kPadding - kTitleHeight - pIndex * kRowHeight;
    tRow.mBottom = tRow.mTop - kRowHeight;
    return tRow;
}
