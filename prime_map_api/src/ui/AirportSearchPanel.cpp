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

bool AirportSearchPanel::isOpen() const
{
    return mOpen;
}

void AirportSearchPanel::setOpen(bool pOpen)
{
    mOpen = pOpen;
    if (!mOpen)
    {
        mQuery.clear();
        mResults.clear();
        mSelected = 0;
    }
}

void AirportSearchPanel::toggle()
{
    setOpen(!mOpen);
}

void AirportSearchPanel::layout(const RectI& pAnchor)
{
    // Sonuc yoksa da bir satirlik yer (bilgi yazisi)
    const int tRows = std::max(1, static_cast<int>(mResults.size()));
    mRect.mLeft = pAnchor.mLeft;
    mRect.mRight = pAnchor.mLeft + kWidth;
    mRect.mTop = pAnchor.mBottom - 4;
    mRect.mBottom = mRect.mTop - (kPadding + kTitleHeight + kInputHeight + 6 + tRows * kRowHeight + kPadding);
}

RectI AirportSearchPanel::inputRect() const
{
    RectI tRect;
    tRect.mLeft = mRect.mLeft + kPadding;
    tRect.mRight = mRect.mRight - kPadding;
    tRect.mTop = mRect.mTop - kPadding - kTitleHeight;
    tRect.mBottom = tRect.mTop - kInputHeight;
    return tRect;
}

RectI AirportSearchPanel::rowRect(int pIndex) const
{
    RectI tRow;
    tRow.mLeft = mRect.mLeft + kPadding;
    tRow.mRight = mRect.mRight - kPadding;
    tRow.mTop = inputRect().mBottom - 6 - pIndex * kRowHeight;
    tRow.mBottom = tRow.mTop - kRowHeight;
    return tRow;
}

void AirportSearchPanel::draw(Gfx::Canvas& pCanvas, int pMouseX, int pMouseY, bool pFocused, float pNow) const
{
    if (!mOpen)
    {
        return;
    }
    Ui::fillRect(pCanvas, mRect, Ui::kPanelBg);
    Ui::frameRect(pCanvas, mRect, Ui::kBorder);
    Ui::text(mRect.mLeft + kPadding, mRect.mTop - kPadding - 12, "GO TO AIRPORT", Ui::kTextGrey);

    // Yazi kutusu: odak bizdeyse acik mavi cerceve ve yanip sonen imlec
    const RectI tInput = inputRect();
    Ui::fillRect(pCanvas, tInput, Ui::kButton);
    Ui::frameRect(pCanvas, tInput, pFocused ? Ui::kButtonActiveBorder : Ui::kBorder);
    const int tTextX = tInput.mLeft + 6;
    const int tTextY = (tInput.mTop + tInput.mBottom) / 2 - Ui::fontHeight() / 2 + 1;
    if (mQuery.empty() && !pFocused)
    {
        Ui::text(tTextX, tTextY, "Click and type ICAO or name", Ui::kTextDim);
    }
    else
    {
        Ui::text(tTextX, tTextY, mQuery, Ui::kTextWhite);
        const bool tCaretOn = std::fmod(pNow, 1.0f) < 0.6f;
        if (pFocused && tCaretOn)
        {
            const float tCaretX = static_cast<float>(tTextX + Ui::textWidth(mQuery) + 1) + 0.5f;
            Ui::line(pCanvas, {tCaretX, static_cast<float>(tInput.mBottom + 4)},
                     {tCaretX, static_cast<float>(tInput.mTop - 4)}, 1.0f, Ui::kTextWhite);
        }
    }

    const int tRowTextOffset = kRowHeight / 2 - Ui::fontHeight() / 2 + 1;
    if (mResults.empty())
    {
        const RectI tRow = rowRect(0);
        const char* tInfo = !mAirports.isReady() ? "Airports are still loading..."
                            : mQuery.empty()     ? "Example: KSEA, LTFM, Tacoma"
                                                 : "No airport found";
        Ui::text(tRow.mLeft + 4, tRow.mBottom + tRowTextOffset, tInfo, Ui::kTextDim);
        return;
    }
    for (int tIndex = 0; tIndex < static_cast<int>(mResults.size()); ++tIndex)
    {
        const RectI tRow = rowRect(tIndex);
        const bool tHover = tRow.contains(pMouseX, pMouseY);
        if (tIndex == mSelected || tHover)
        {
            Ui::fillRect(pCanvas, tRow, tIndex == mSelected ? Ui::kButtonActive : Ui::kButtonHover);
        }
        const AirportGeometry& tAirport = mAirports.at(mResults[tIndex]);
        const int tY = tRow.mBottom + tRowTextOffset;
        Ui::text(tRow.mLeft + 4, tY, tAirport.mId, Ui::kTextWhite);
        // Ad: satira sigmazsa sondan kisaltilir
        std::string tName = tAirport.mName;
        const int tNameWidth = tRow.width() - kIdColumn - 8;
        while (!tName.empty() && Ui::textWidth(tName) > tNameWidth)
        {
            tName.pop_back();
        }
        if (tName.size() < tAirport.mName.size() && tName.size() > 2)
        {
            tName.resize(tName.size() - 2);
            tName += "..";
        }
        Ui::text(tRow.mLeft + kIdColumn, tY, tName, tIndex == mSelected ? Ui::kTextWhite : Ui::kTextGrey);
    }
}

AirportSearchPanel::Hit AirportSearchPanel::hitTest(int pX, int pY) const
{
    Hit tHit;
    if (!mOpen || !mRect.contains(pX, pY))
    {
        return tHit;
    }
    tHit.mInside = true;
    tHit.mInput = inputRect().contains(pX, pY);
    for (int tIndex = 0; tIndex < static_cast<int>(mResults.size()); ++tIndex)
    {
        if (rowRect(tIndex).contains(pX, pY))
        {
            tHit.mAirport = mResults[tIndex];
        }
    }
    return tHit;
}

void AirportSearchPanel::typeCharacter(char pChar)
{
    if (pChar < ' ' || pChar > '~' || mQuery.size() >= kMaxQueryLength)
    {
        return;
    }
    mQuery += pChar;
    search();
}

void AirportSearchPanel::backspace()
{
    if (!mQuery.empty())
    {
        mQuery.pop_back();
        search();
    }
}

void AirportSearchPanel::moveSelection(int pDelta)
{
    if (!mResults.empty())
    {
        mSelected = std::clamp(mSelected + pDelta, 0, static_cast<int>(mResults.size()) - 1);
    }
}

int AirportSearchPanel::selectedAirport() const
{
    return mResults.empty() ? -1 : mResults[mSelected];
}

void AirportSearchPanel::refresh()
{
    if (mOpen && !mSearchedWhenReady && mAirports.isReady() && !mQuery.empty())
    {
        search();
    }
}

void AirportSearchPanel::search()
{
    mResults = findAirports(mAirports, mQuery, kMaxResults);
    mSelected = 0;
    mSearchedWhenReady = mAirports.isReady();
}
