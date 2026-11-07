// AirportSearchPanel.cpp
#include "ui/AirportSearchPanel.h"

#include <algorithm>
#include <cmath>

#include "data/AirportGeometryStore.h"
#include "data/AirportSearch.h"
#include "ui/UiDraw.h"

namespace
{

const int kPadding = 8;
const int kTitleHeight = 20;
const int kInputHeight = 22;
const int kRowHeight = 20;
const int kWidth = 320;
const int kMaxResults = 8;
const size_t kMaxQueryLength = 40;
const int kIdColumn = 52;  // kimlik sutununun genisligi (ad bunun saginda)

}  // namespace

AirportSearchPanel::AirportSearchPanel(const AirportGeometryStore& pAirports)
    : mAirports(pAirports)
{
}

bool AirportSearchPanel::IsOpen() const
{
    return mOpen;
}

void AirportSearchPanel::SetOpen(bool pOpen)
{
    mOpen = pOpen;
    if (!mOpen)
    {
        mQuery.clear();
        mResults.clear();
        mSelected = 0;
    }
}

void AirportSearchPanel::Toggle()
{
    SetOpen(!mOpen);
}

void AirportSearchPanel::Layout(const RectI& pAnchor)
{
    // Sonuc yoksa da bir satirlik yer (bilgi yazisi)
    const int tRows = std::max(1, static_cast<int>(mResults.size()));
    mRect.mLeft = pAnchor.mLeft;
    mRect.mRight = pAnchor.mLeft + kWidth;
    mRect.mTop = pAnchor.mBottom - 4;
    mRect.mBottom = mRect.mTop - (kPadding + kTitleHeight + kInputHeight + 6 + tRows * kRowHeight + kPadding);
}

RectI AirportSearchPanel::InputRect() const
{
    RectI tRect;
    tRect.mLeft = mRect.mLeft + kPadding;
    tRect.mRight = mRect.mRight - kPadding;
    tRect.mTop = mRect.mTop - kPadding - kTitleHeight;
    tRect.mBottom = tRect.mTop - kInputHeight;
    return tRect;
}

RectI AirportSearchPanel::RowRect(int pIndex) const
{
    RectI tRow;
    tRow.mLeft = mRect.mLeft + kPadding;
    tRow.mRight = mRect.mRight - kPadding;
    tRow.mTop = InputRect().mBottom - 6 - pIndex * kRowHeight;
    tRow.mBottom = tRow.mTop - kRowHeight;
    return tRow;
}

void AirportSearchPanel::Draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, bool pFocused, float pNow) const
{
    if (!mOpen)
    {
        return;
    }
    Ui::FillRect(pCanvas, mRect, Ui::kPanelBg);
    Ui::FrameRect(pCanvas, mRect, Ui::kBorder);
    Ui::Text(mRect.mLeft + kPadding, mRect.mTop - kPadding - 12, "GO TO AIRPORT", Ui::kTextGrey);

    // Yazi kutusu: odak bizdeyse acik mavi cerceve ve yanip sonen imlec
    const RectI tInput = InputRect();
    Ui::FillRect(pCanvas, tInput, Ui::kButton);
    Ui::FrameRect(pCanvas, tInput, pFocused ? Ui::kButtonActiveBorder : Ui::kBorder);
    const int tTextX = tInput.mLeft + 6;
    const int tTextY = (tInput.mTop + tInput.mBottom) / 2 - Ui::FontHeight() / 2 + 1;
    if (mQuery.empty() && !pFocused)
    {
        Ui::Text(tTextX, tTextY, "Click and type ICAO or name", Ui::kTextDim);
    }
    else
    {
        Ui::Text(tTextX, tTextY, mQuery, Ui::kTextWhite);
        const bool tCaretOn = std::fmod(pNow, 1.0f) < 0.6f;
        if (pFocused && tCaretOn)
        {
            const float tCaretX = static_cast<float>(tTextX + Ui::TextWidth(mQuery) + 1) + 0.5f;
            Ui::Line(pCanvas, {tCaretX, static_cast<float>(tInput.mBottom + 4)},
                     {tCaretX, static_cast<float>(tInput.mTop - 4)}, 1.0f, Ui::kTextWhite);
        }
    }

    const int tRowTextOffset = kRowHeight / 2 - Ui::FontHeight() / 2 + 1;
    if (mResults.empty())
    {
        const RectI tRow = RowRect(0);
        const char* tInfo = !mAirports.IsReady() ? "Airports are still loading..."
                            : mQuery.empty()     ? "Example: KSEA, LTFM, Tacoma"
                                                 : "No airport found";
        Ui::Text(tRow.mLeft + 4, tRow.mBottom + tRowTextOffset, tInfo, Ui::kTextDim);
        return;
    }
    for (int tIndex = 0; tIndex < static_cast<int>(mResults.size()); ++tIndex)
    {
        const RectI tRow = RowRect(tIndex);
        const bool tHover = tRow.Contains(pMouseX, pMouseY);
        if (tIndex == mSelected || tHover)
        {
            Ui::FillRect(pCanvas, tRow, tIndex == mSelected ? Ui::kButtonActive : Ui::kButtonHover);
        }
        const AirportGeometry& tAirport = mAirports.At(mResults[tIndex]);
        const int tY = tRow.mBottom + tRowTextOffset;
        Ui::Text(tRow.mLeft + 4, tY, tAirport.mId, Ui::kTextWhite);
        // Ad: satira sigmazsa sondan kisaltilir
        std::string tName = tAirport.mName;
        const int tNameWidth = tRow.Width() - kIdColumn - 8;
        while (!tName.empty() && Ui::TextWidth(tName) > tNameWidth)
        {
            tName.pop_back();
        }
        if (tName.size() < tAirport.mName.size() && tName.size() > 2)
        {
            tName.resize(tName.size() - 2);
            tName += "..";
        }
        Ui::Text(tRow.mLeft + kIdColumn, tY, tName, tIndex == mSelected ? Ui::kTextWhite : Ui::kTextGrey);
    }
}

AirportSearchPanel::Hit AirportSearchPanel::HitTest(int pX, int pY) const
{
    Hit tHit;
    if (!mOpen || !mRect.Contains(pX, pY))
    {
        return tHit;
    }
    tHit.mInside = true;
    tHit.mInput = InputRect().Contains(pX, pY);
    for (int tIndex = 0; tIndex < static_cast<int>(mResults.size()); ++tIndex)
    {
        if (RowRect(tIndex).Contains(pX, pY))
        {
            tHit.mAirport = mResults[tIndex];
        }
    }
    return tHit;
}

void AirportSearchPanel::TypeCharacter(char pChar)
{
    if (pChar < ' ' || pChar > '~' || mQuery.size() >= kMaxQueryLength)
    {
        return;
    }
    mQuery += pChar;
    Search();
}

void AirportSearchPanel::Backspace()
{
    if (!mQuery.empty())
    {
        mQuery.pop_back();
        Search();
    }
}

void AirportSearchPanel::MoveSelection(int pDelta)
{
    if (!mResults.empty())
    {
        mSelected = std::clamp(mSelected + pDelta, 0, static_cast<int>(mResults.size()) - 1);
    }
}

int AirportSearchPanel::SelectedAirport() const
{
    return mResults.empty() ? -1 : mResults[mSelected];
}

void AirportSearchPanel::Refresh()
{
    if (mOpen && !mSearchedWhenReady && mAirports.IsReady() && !mQuery.empty())
    {
        Search();
    }
}

void AirportSearchPanel::Search()
{
    mResults = FindAirports(mAirports, mQuery, kMaxResults);
    mSelected = 0;
    mSearchedWhenReady = mAirports.IsReady();
}
