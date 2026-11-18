#include "ui/StatusBar.h"

#include <format>
#include <string>

#include "XMapFormat.h"
#include "XMapView.h"
#include "gfx/Canvas.h"
#include "ui/UiDraw.h"

namespace
{
const int kPadding = 8;
const double kMetersPerNm = 1852.0;
const double kScaleBarMaxBoxels = 120.0;
}

void StatusBar::draw(Gfx::Canvas& pCanvas, const RectI& pArea, const XMapView& pView,
                     const std::optional<LatLon>& pCursor, std::string_view pMessage)
{
    mStrip.mLeft = pArea.mLeft;
    mStrip.mRight = pArea.mRight;
    mStrip.mBottom = pArea.mBottom;
    mStrip.mTop = pArea.mBottom + kHeight;

    Ui::fillRect(pCanvas, mStrip, Ui::kStripBg);
    Ui::line(pCanvas, {static_cast<float>(mStrip.mLeft), mStrip.mTop - 0.5f},
             {static_cast<float>(mStrip.mRight), mStrip.mTop - 0.5f}, 1.0f, Ui::kBorder);

    const int tBaseY = (mStrip.mTop + mStrip.mBottom) / 2 - Ui::fontHeight() / 2 + 1;

    if (pCursor)
    {
        Ui::text(mStrip.mLeft + kPadding, tBaseY, formatLatLon(*pCursor), Ui::kTextWhite);
    }
    else
    {
        Ui::text(mStrip.mLeft + kPadding, tBaseY, "-", Ui::kTextDim);
    }

    const std::string tZoom = std::format("Zoom {:.1f}", pView.zoom());
    const int tZoomWidth = Ui::textWidth(tZoom);
    const int tZoomX = (mStrip.mLeft + mStrip.mRight) / 2 - tZoomWidth / 2;
    Ui::text(tZoomX, tBaseY, tZoom, Ui::kTextGrey);

    if (!pMessage.empty())
    {
        Ui::text(tZoomX + tZoomWidth + 24, tBaseY, pMessage, Ui::kTextYellow);
    }

    drawScaleBar(pCanvas, pView, mStrip.mRight - kPadding, tBaseY);
}

bool StatusBar::contains(int pX, int pY) const
{
    return mStrip.contains(pX, pY);
}

void StatusBar::drawScaleBar(Gfx::Canvas& pCanvas, const XMapView& pView, int pRight, int pBaseY)
{
    const double tNmPerBoxel = pView.metersPerBoxel() / kMetersPerNm;
    const double tLengthNm = chooseScaleBarNm(tNmPerBoxel, kScaleBarMaxBoxels);
    if (tLengthNm <= 0.0)
    {
        return;
    }
    const float tLengthBoxels = static_cast<float>(tLengthNm / tNmPerBoxel);

    const std::string tLabel =
        tLengthNm >= 1.0 ? std::format("{:.0f} NM", tLengthNm) : std::format("{:.1f} NM", tLengthNm);
    const int tLabelWidth = Ui::textWidth(tLabel);
    const float tBarRight = static_cast<float>(pRight - tLabelWidth - 8);
    const float tBarLeft = tBarRight - tLengthBoxels;
    const float tBarY = pBaseY + 2.0f;

    Gfx::Mesh tBar;
    tBar.addLine({tBarLeft, tBarY}, {tBarRight, tBarY}, 2.0f, Ui::kTextWhite);
    tBar.addLine({tBarLeft, tBarY - 3.0f}, {tBarLeft, tBarY + 5.0f}, 2.0f, Ui::kTextWhite);
    tBar.addLine({tBarRight, tBarY - 3.0f}, {tBarRight, tBarY + 5.0f}, 2.0f, Ui::kTextWhite);
    pCanvas.draw(tBar);
    Ui::text(pRight - tLabelWidth, pBaseY, tLabel, Ui::kTextWhite);
}
