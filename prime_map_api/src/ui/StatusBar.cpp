// StatusBar.cpp
#include "ui/StatusBar.h"

#include <format>
#include <string>

#include "MapFormat.h"
#include "MapView.h"
#include "gfx/Canvas.h"
#include "ui/UiDraw.h"

namespace
{

const int kPadding = 8;
const double kMetersPerNm = 1852.0;
const double kScaleBarMaxBoxels = 120.0;

}  // namespace

void StatusBar::Draw(Gfx::Canvas& pCanvas, const RectI& pArea, const MapView& pView,
                     const std::optional<LatLon>& pCursor, std::string_view pMessage)
{
    mStrip.mLeft = pArea.mLeft;
    mStrip.mRight = pArea.mRight;
    mStrip.mBottom = pArea.mBottom;
    mStrip.mTop = pArea.mBottom + kHeight;

    Ui::FillRect(pCanvas, mStrip, Ui::kStripBg);
    Ui::Line(pCanvas, {static_cast<float>(mStrip.mLeft), mStrip.mTop - 0.5f},
             {static_cast<float>(mStrip.mRight), mStrip.mTop - 0.5f}, 1.0f, Ui::kBorder);

    const int tBaseY = (mStrip.mTop + mStrip.mBottom) / 2 - Ui::FontHeight() / 2 + 1;

    // Sol: imlecin altindaki nokta
    if (pCursor)
    {
        Ui::Text(mStrip.mLeft + kPadding, tBaseY, FormatLatLon(*pCursor), Ui::kTextWhite);
    }
    else
    {
        Ui::Text(mStrip.mLeft + kPadding, tBaseY, "-", Ui::kTextDim);
    }

    // Orta: zoom seviyesi
    const std::string tZoom = std::format("Zoom {:.1f}", pView.Zoom());
    const int tZoomWidth = Ui::TextWidth(tZoom);
    const int tZoomX = (mStrip.mLeft + mStrip.mRight) / 2 - tZoomWidth / 2;
    Ui::Text(tZoomX, tBaseY, tZoom, Ui::kTextGrey);

    // Zoom'un sagi: durum yazisi (yukleme, teshis)
    if (!pMessage.empty())
    {
        Ui::Text(tZoomX + tZoomWidth + 24, tBaseY, pMessage, Ui::kTextYellow);
    }

    // Sag: olcek cubugu
    DrawScaleBar(pCanvas, pView, mStrip.mRight - kPadding, tBaseY);
}

bool StatusBar::Contains(int pX, int pY) const
{
    return mStrip.Contains(pX, pY);
}

void StatusBar::DrawScaleBar(Gfx::Canvas& pCanvas, const MapView& pView, int pRight, int pBaseY)
{
    const double tNmPerBoxel = pView.MetersPerBoxel() / kMetersPerNm;
    const double tLengthNm = ChooseScaleBarNm(tNmPerBoxel, kScaleBarMaxBoxels);
    if (tLengthNm <= 0.0)
    {
        return;
    }
    const float tLengthBoxels = static_cast<float>(tLengthNm / tNmPerBoxel);

    const std::string tLabel =
        tLengthNm >= 1.0 ? std::format("{:.0f} NM", tLengthNm) : std::format("{:.1f} NM", tLengthNm);
    const int tLabelWidth = Ui::TextWidth(tLabel);
    const float tBarRight = static_cast<float>(pRight - tLabelWidth - 8);
    const float tBarLeft = tBarRight - tLengthBoxels;
    const float tBarY = pBaseY + 2.0f;

    // |-------|  seklinde: yatay cizgi + iki uc cizgisi
    Gfx::Mesh tBar;
    tBar.AddLine({tBarLeft, tBarY}, {tBarRight, tBarY}, 2.0f, Ui::kTextWhite);
    tBar.AddLine({tBarLeft, tBarY - 3.0f}, {tBarLeft, tBarY + 5.0f}, 2.0f, Ui::kTextWhite);
    tBar.AddLine({tBarRight, tBarY - 3.0f}, {tBarRight, tBarY + 5.0f}, 2.0f, Ui::kTextWhite);
    pCanvas.Draw(tBar);
    Ui::Text(pRight - tLabelWidth, pBaseY, tLabel, Ui::kTextWhite);
}
