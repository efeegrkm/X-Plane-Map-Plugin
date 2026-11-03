// AirwayLayer.cpp
#include "layers/AirwayLayer.h"

#include <cmath>

#include "LabelPlacer.h"
#include "MapView.h"
#include "data/AirwayDatabase.h"
#include "gfx/Batch.h"
#include "layers/MapSymbols.h"

namespace
{

const double kMinZoom = 6.5;
const double kLabelMinZoom = 8.0;
const double kArrowMinZoom = 9.0;
const double kDensifyNm = 20.0;        // uzun segmentler buyuk daire boyunca bolunur
const size_t kMaxSegments = 6000;      // FPS korumasi
const float kMinLabelLength = 90.0f;   // ekranda bundan kisa segmente ad yazilmaz (boxel)
const double kEdgeMarginDeg = 0.2;

const Rgba kLowColor = {0.42f, 0.68f, 0.52f, 0.85f};
const Rgba kHighColor = {0.50f, 0.58f, 0.88f, 0.85f};
const Rgba kLowLabel = {0.62f, 0.85f, 0.70f, 1.0f};
const Rgba kHighLabel = {0.70f, 0.76f, 0.98f, 1.0f};

}  // namespace

AirwayLayer::AirwayLayer(const AirwayDatabase& pAirways, bool pHigh)
    : Layer(pHigh ? "airways_high" : "airways_low", pHigh ? "Airways (high)" : "Airways (low)", true)
    , mAirways(pAirways)
    , mHigh(pHigh)
{
}

void AirwayLayer::Draw(const DrawContext& pContext)
{
    mVisible.clear();
    const MapView& tView = *pContext.mView;
    if (!mAirways.IsReady() || tView.Zoom() < kMinZoom)
    {
        return;
    }
    if (mSeenStamp.size() != static_cast<size_t>(mAirways.Count()))
    {
        mSeenStamp.assign(mAirways.Count(), 0);
    }
    ++mStamp;

    GeoBounds tBounds = tView.VisibleBounds();
    tBounds.mSouth -= kEdgeMarginDeg;
    tBounds.mNorth += kEdgeMarginDeg;
    tBounds.mWest -= kEdgeMarginDeg;
    tBounds.mEast += kEdgeMarginDeg;
    mQuery.clear();
    mAirways.Query(tBounds, mQuery);

    const Rgba& tColor = mHigh ? kHighColor : kLowColor;
    const bool tArrows = tView.Zoom() >= kArrowMinZoom;
    Gfx::LineBatch tLines;
    for (int tIndex : mQuery)
    {
        if (mSeenStamp[tIndex] == mStamp)
        {
            continue;  // uzun segment: baska bir hucreden zaten geldi
        }
        mSeenStamp[tIndex] = mStamp;
        const AirwaySegment& tSegment = mAirways.At(tIndex);
        if (tSegment.mHigh != mHigh)
        {
            continue;
        }

        std::vector<LatLon> tDense;
        DensifySegment(tSegment.mFrom, tSegment.mTo, kDensifyNm, tDense);
        tDense.push_back(tSegment.mTo);
        std::vector<Gfx::Point> tScreen;
        tScreen.reserve(tDense.size());
        for (const LatLon& tPoint : tDense)
        {
            double tX = 0.0;
            double tY = 0.0;
            tView.LatLonToBoxel(tPoint, &tX, &tY);
            tScreen.push_back({static_cast<float>(tX), static_cast<float>(tY)});
        }
        tLines.AddPolyline(tScreen, false, tColor);

        VisibleSegment tVisible;
        tVisible.mIndex = tIndex;
        tVisible.mFromX = tScreen.front().mX;
        tVisible.mFromY = tScreen.front().mY;
        tVisible.mToX = tScreen.back().mX;
        tVisible.mToY = tScreen.back().mY;
        mVisible.push_back(tVisible);

        // Tek yonlu segment: ortada yon oku (">" seklinde iki kisa cizgi)
        if (tArrows && tSegment.mDirection != 'N')
        {
            const float tSign = (tSegment.mDirection == 'F') ? 1.0f : -1.0f;
            const float tDeltaX = (tVisible.mToX - tVisible.mFromX) * tSign;
            const float tDeltaY = (tVisible.mToY - tVisible.mFromY) * tSign;
            const float tLength = std::sqrt(tDeltaX * tDeltaX + tDeltaY * tDeltaY);
            if (tLength > 40.0f)
            {
                const float tDirX = tDeltaX / tLength;
                const float tDirY = tDeltaY / tLength;
                const Gfx::Point tTip = {(tVisible.mFromX + tVisible.mToX) * 0.5f + tDirX * 5.0f,
                                         (tVisible.mFromY + tVisible.mToY) * 0.5f + tDirY * 5.0f};
                tLines.AddLine(tTip, {tTip.mX - tDirX * 7.0f - tDirY * 4.0f, tTip.mY - tDirY * 7.0f + tDirX * 4.0f},
                               tColor);
                tLines.AddLine(tTip, {tTip.mX - tDirX * 7.0f + tDirY * 4.0f, tTip.mY - tDirY * 7.0f - tDirX * 4.0f},
                               tColor);
            }
        }
        if (mVisible.size() >= kMaxSegments)
        {
            break;
        }
    }
    tLines.Draw(1.2f);
}

void AirwayLayer::DrawLabels(const DrawContext& pContext)
{
    if (!pContext.mLabels || pContext.mView->Zoom() < kLabelMinZoom)
    {
        return;
    }
    const Rgba& tColor = mHigh ? kHighLabel : kLowLabel;
    for (const VisibleSegment& tVisible : mVisible)
    {
        const float tDeltaX = tVisible.mToX - tVisible.mFromX;
        const float tDeltaY = tVisible.mToY - tVisible.mFromY;
        if (std::sqrt(tDeltaX * tDeltaX + tDeltaY * tDeltaY) < kMinLabelLength)
        {
            continue;
        }
        const AirwaySegment& tSegment = mAirways.At(tVisible.mIndex);
        MapSymbols::DrawLabelCentered(*pContext.mLabels, (tVisible.mFromX + tVisible.mToX) * 0.5f,
                                      (tVisible.mFromY + tVisible.mToY) * 0.5f + 8.0f, tSegment.mNames.c_str(),
                                      tColor);
    }
}
