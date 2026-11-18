#include "layers/RunwayLayer.h"

#include <cmath>
#include <format>

#include "Geo.h"
#include "XMapView.h"
#include "data/AirportGeometryStore.h"
#include "gfx/Canvas.h"
#include "layers/XMapSymbols.h"

namespace
{
const double kLabelMinZoom = 11.5;
const double kOverrunMinZoom = 12.0;
const double kMarkingMinZoom = 13.0;
const double kQueryMarginDeg = 0.1;
const double kEdgeMargin = 40.0;
const float kMinHalfWidth = 1.0f;
const float kEdgeWidth = 2.0f;

enum RunwayPart
{
    kPartPaved,
    kPartUnpaved,
    kPartWater,
    kPartOverruns,
    kPartMarkings,
    kPartNumbers,
};

int partOf(const Runway& pRunway)
{
    if (pRunway.mWater || pRunway.mSurface == RunwaySurface::Water)
    {
        return kPartWater;
    }
    return pRunway.mSurface == RunwaySurface::Paved ? kPartPaved : kPartUnpaved;
}
const float kMinLength = 3.0f;
const float kLabelGap = 10.0f;
const double kDashMeters = 30.0;

const Rgba kPavedColor = {0.22f, 0.23f, 0.26f, 1.0f};
const Rgba kUnpavedColor = {0.52f, 0.44f, 0.30f, 1.0f};
const Rgba kWaterColor = {0.30f, 0.50f, 0.80f, 0.85f};
const Rgba kSnowColor = {0.85f, 0.88f, 0.92f, 1.0f};
const Rgba kOverrunColor = {0.33f, 0.34f, 0.37f, 1.0f};
const Rgba kMarkingColor = {0.96f, 0.97f, 0.98f, 1.0f};

const Rgba& surfaceColor(RunwaySurface pSurface)
{
    switch (pSurface)
    {
        case RunwaySurface::Unpaved:
            return kUnpavedColor;
        case RunwaySurface::Water:
            return kWaterColor;
        case RunwaySurface::Snow:
            return kSnowColor;
        default:
            return kPavedColor;
    }
}
}

RunwayLayer::RunwayLayer(const AirportGeometryStore& pStore)
    : Layer("runways", "Runways", true)
    , mStore(pStore)
{
    addSubLayer("paved", "Paved (land)");
    addSubLayer("unpaved", "Unpaved (land)");
    addSubLayer("water", "Water runways");
    addSubLayer("overruns", "Blast pads / overruns");
    addSubLayer("markings", "Markings");
    addSubLayer("numbers", "Runway numbers");
}

void RunwayLayer::collectVisible(const drawContext& pContext)
{
    mVisible.clear();
    const XMapView& tView = *pContext.mView;
    if (!mStore.isReady() || !isInZoomRange())
    {
        return;
    }

    RectD tArea = pContext.mArea;
    tArea.mLeft -= kEdgeMargin;
    tArea.mRight += kEdgeMargin;
    tArea.mBottom -= kEdgeMargin;
    tArea.mTop += kEdgeMargin;
    GeoBounds tBounds = tView.boundsOf(tArea);
    tBounds.mSouth -= kQueryMarginDeg;
    tBounds.mNorth += kQueryMarginDeg;
    tBounds.mWest -= kQueryMarginDeg;
    tBounds.mEast += kQueryMarginDeg;

    mQuery.clear();
    mStore.query(tBounds, mQuery);

    for (int tAirportIndex : mQuery)
    {
        for (const Runway& tRunway : mStore.at(tAirportIndex).mRunways)
        {
            if (!isSubLayerDrawn(partOf(tRunway)))
            {
                continue;
            }
            ScreenRunway tScreen;
            tScreen.mRunway = &tRunway;
            for (int tEnd = 0; tEnd < 2; ++tEnd)
            {
                const PointF tPoint = tView.toScreen(tRunway.mEnds[tEnd].mPosition);
                tScreen.mEndX[tEnd] = tPoint.mX;
                tScreen.mEndY[tEnd] = tPoint.mY;
            }
            const float tDeltaX = tScreen.mEndX[1] - tScreen.mEndX[0];
            const float tDeltaY = tScreen.mEndY[1] - tScreen.mEndY[0];
            tScreen.mLength = std::sqrt(tDeltaX * tDeltaX + tDeltaY * tDeltaY);
            if (tScreen.mLength < kMinLength)
            {
                continue;
            }

            const float tMinX = std::fmin(tScreen.mEndX[0], tScreen.mEndX[1]);
            const float tMaxX = std::fmax(tScreen.mEndX[0], tScreen.mEndX[1]);
            const float tMinY = std::fmin(tScreen.mEndY[0], tScreen.mEndY[1]);
            const float tMaxY = std::fmax(tScreen.mEndY[0], tScreen.mEndY[1]);
            if (tMaxX < tArea.mLeft || tMinX > tArea.mRight || tMaxY < tArea.mBottom || tMinY > tArea.mTop)
            {
                continue;
            }

            tScreen.mDirX = tDeltaX / tScreen.mLength;
            tScreen.mDirY = tDeltaY / tScreen.mLength;
            const double tMidLat = (tRunway.mEnds[0].mPosition.mLat + tRunway.mEnds[1].mPosition.mLat) * 0.5;
            tScreen.mBoxelsPerMeter = static_cast<float>(1.0 / tView.metersPerBoxelAt(tMidLat));
            tScreen.mHalfWidth =
                std::fmax(static_cast<float>(tRunway.mWidthM) * tScreen.mBoxelsPerMeter * 0.5f, kMinHalfWidth);
            mVisible.push_back(tScreen);
        }
    }
}

void RunwayLayer::addRectangle(Gfx::Mesh& pMesh, const ScreenRunway& pRunway, float pFrom, float pTo,
                               float pHalfWidth, const Rgba& pColor)
{
    const float tNormalX = -pRunway.mDirY * pHalfWidth;
    const float tNormalY = pRunway.mDirX * pHalfWidth;
    const float tStartX = pRunway.mEndX[0] + pRunway.mDirX * pFrom;
    const float tStartY = pRunway.mEndY[0] + pRunway.mDirY * pFrom;
    const float tStopX = pRunway.mEndX[0] + pRunway.mDirX * pTo;
    const float tStopY = pRunway.mEndY[0] + pRunway.mDirY * pTo;
    pMesh.addQuad({tStartX + tNormalX, tStartY + tNormalY}, {tStartX - tNormalX, tStartY - tNormalY},
                  {tStopX - tNormalX, tStopY - tNormalY}, {tStopX + tNormalX, tStopY + tNormalY}, pColor);
}

void RunwayLayer::draw(const drawContext& pContext)
{
    collectVisible(pContext);
    if (mVisible.empty())
    {
        return;
    }
    const double tZoom = pContext.mView->zoom();
    Gfx::Mesh tMesh;

    if (tZoom >= kOverrunMinZoom && isSubLayerDrawn(kPartOverruns))
    {
        for (const ScreenRunway& tRunway : mVisible)
        {
            const float tOverrun0 = static_cast<float>(tRunway.mRunway->mEnds[0].mOverrunM) * tRunway.mBoxelsPerMeter;
            const float tOverrun1 = static_cast<float>(tRunway.mRunway->mEnds[1].mOverrunM) * tRunway.mBoxelsPerMeter;
            if (tOverrun0 > 0.5f)
            {
                addRectangle(tMesh, tRunway, -tOverrun0, 0.0f, tRunway.mHalfWidth, kOverrunColor);
            }
            if (tOverrun1 > 0.5f)
            {
                addRectangle(tMesh, tRunway, tRunway.mLength, tRunway.mLength + tOverrun1, tRunway.mHalfWidth,
                             kOverrunColor);
            }
        }
    }

    const XMapTheme& tTheme = *pContext.mTheme;
    const Rgba& tEdgeColor = tTheme.mRunwayEdge;
    for (const ScreenRunway& tRunway : mVisible)
    {
        if (tRunway.mHalfWidth >= 2.0f)
        {
            addRectangle(tMesh, tRunway, -kEdgeWidth, tRunway.mLength + kEdgeWidth, tRunway.mHalfWidth + kEdgeWidth,
                         tEdgeColor);
        }
    }
    for (const ScreenRunway& tRunway : mVisible)
    {
        if (tRunway.mRunway->mSurface != RunwaySurface::Transparent)
        {
            const bool tThin = tRunway.mHalfWidth < 2.0f;
            addRectangle(tMesh, tRunway, 0.0f, tRunway.mLength, tRunway.mHalfWidth,
                         tThin && !tTheme.mLight ? tEdgeColor : surfaceColor(tRunway.mRunway->mSurface));
        }
    }

    if (tZoom >= kMarkingMinZoom && isSubLayerDrawn(kPartMarkings))
    {
        for (const ScreenRunway& tRunway : mVisible)
        {
            if (tRunway.mRunway->mSurface == RunwaySurface::Paved && tRunway.mHalfWidth >= 3.0f)
            {
                addMarkings(tMesh, tRunway);
            }
        }
    }
    pContext.mCanvas->draw(tMesh);
}

void RunwayLayer::addMarkings(Gfx::Mesh& pMesh, const ScreenRunway& pRunway)
{
    const float tThreshold0 = static_cast<float>(pRunway.mRunway->mEnds[0].mDisplacedThresholdM) *
                              pRunway.mBoxelsPerMeter;
    const float tThreshold1 = pRunway.mLength - static_cast<float>(pRunway.mRunway->mEnds[1].mDisplacedThresholdM) *
                                                    pRunway.mBoxelsPerMeter;
    const float tBar = 1.5f;
    addRectangle(pMesh, pRunway, tThreshold0, tThreshold0 + tBar, pRunway.mHalfWidth * 0.85f, kMarkingColor);
    addRectangle(pMesh, pRunway, tThreshold1 - tBar, tThreshold1, pRunway.mHalfWidth * 0.85f, kMarkingColor);

    const float tDash = std::fmax(static_cast<float>(kDashMeters) * pRunway.mBoxelsPerMeter, 3.0f);
    const float tLineHalf = std::fmax(pRunway.mHalfWidth * 0.03f, 0.5f);
    for (float tAt = tThreshold0 + tDash; tAt + tDash < tThreshold1 - tDash; tAt += 2.0f * tDash)
    {
        addRectangle(pMesh, pRunway, tAt, tAt + tDash, tLineHalf, kMarkingColor);
    }
}

void RunwayLayer::drawLabels(const drawContext& pContext)
{
    if (!pContext.mLabels || pContext.mView->zoom() < kLabelMinZoom || !isSubLayerDrawn(kPartNumbers))
    {
        return;
    }
    for (const ScreenRunway& tRunway : mVisible)
    {
        for (int tEnd = 0; tEnd < 2; ++tEnd)
        {
            const float tSign = (tEnd == 0) ? -1.0f : 1.0f;
            const float tOverrun =
                static_cast<float>(tRunway.mRunway->mEnds[tEnd].mOverrunM) * tRunway.mBoxelsPerMeter;
            const float tOffset = kLabelGap + tOverrun;
            const float tX = tRunway.mEndX[tEnd] + tSign * tRunway.mDirX * tOffset;
            const float tY = tRunway.mEndY[tEnd] + tSign * tRunway.mDirY * tOffset;
            XMapSymbols::drawLabelCentered(*pContext.mLabels, tX, tY, tRunway.mRunway->mEnds[tEnd].mName,
                                          pContext.mTheme->mRunwayLabel, pContext.labelShadow());
        }
    }
}

std::string RunwayLayer::describe(const drawContext& , double pX, double pY) const
{
    for (const ScreenRunway& tRunway : mVisible)
    {
        const float tDeltaX = static_cast<float>(pX) - tRunway.mEndX[0];
        const float tDeltaY = static_cast<float>(pY) - tRunway.mEndY[0];
        const float tAlong = tDeltaX * tRunway.mDirX + tDeltaY * tRunway.mDirY;
        const float tAcross = std::fabs(-tDeltaX * tRunway.mDirY + tDeltaY * tRunway.mDirX);
        if (tAlong < 0.0f || tAlong > tRunway.mLength || tAcross > tRunway.mHalfWidth + 2.0f)
        {
            continue;
        }
        const Runway& tData = *tRunway.mRunway;
        const double tLengthM = distanceNm(tData.mEnds[0].mPosition, tData.mEnds[1].mPosition) * 1852.0;
        return std::format("{} {}/{}  {:.0f} x {:.0f} m, {}", tData.mWater ? "Water runway" : "Runway",
                           tData.mEnds[0].mName, tData.mEnds[1].mName, tLengthM, tData.mWidthM,
                           surfaceName(tData.mSurfaceCode));
    }
    return "";
}
