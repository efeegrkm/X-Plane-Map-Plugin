#include "layers/TaxiRouteLayer.h"

#include <cmath>

#include "LabelPlacer.h"
#include "XMapView.h"
#include "data/AirportDetailStore.h"
#include "data/AirportGeometryStore.h"
#include "gfx/Canvas.h"
#include "layers/AirportVisibility.h"
#include "layers/XMapSymbols.h"

namespace
{
const float kMinLabelLength = 40.0f;
const Rgba kTaxiway = {0.85f, 0.45f, 1.00f, 0.75f};
const Rgba kRunwayCrossing = {1.00f, 0.35f, 0.35f, 0.75f};
const Rgba kLabel = {0.95f, 0.80f, 1.00f, 1.0f};

double segmentDistance(const PointF& pFrom, const PointF& pTo, double pX, double pY)
{
    const double tDx = pTo.mX - pFrom.mX;
    const double tDy = pTo.mY - pFrom.mY;
    const double tLength2 = tDx * tDx + tDy * tDy;
    double tT = tLength2 > 0.0 ? ((pX - pFrom.mX) * tDx + (pY - pFrom.mY) * tDy) / tLength2 : 0.0;
    tT = std::fmax(0.0, std::fmin(1.0, tT));
    return std::hypot(pFrom.mX + tT * tDx - pX, pFrom.mY + tT * tDy - pY);
}
}

TaxiRouteLayer::TaxiRouteLayer(const AirportGeometryStore& pSummaries, AirportDetailStore& pDetails)
    : Layer("taxi_routes", "Taxi routes", false)
    , mSummaries(pSummaries)
    , mDetails(pDetails)
{
}

void TaxiRouteLayer::draw(const drawContext& pContext)
{
    mEdges.clear();
    const XMapView& tView = *pContext.mView;
    if (!isInZoomRange())
    {
        return;
    }
    findVisibleAirports(tView, mSummaries, mQuery, mVisible);
    const float tScale = static_cast<float>(tView.worldSize());
    Gfx::Mesh tLines;
    for (int tIndex : mVisible)
    {
        const LoadedAirport* tLoaded = mDetails.use(tIndex);
        if (!tLoaded)
        {
            continue;
        }
        const AirportShapes& tShapes = tLoaded->mShapes;
        const PointF tOffset = tView.mercatorToScreen(tShapes.mOriginX, tShapes.mOriginY);
        for (const TaxiEdgeShape& tEdge : tShapes.mTaxiEdges)
        {
            ScreenEdge tScreen;
            tScreen.mFrom = {tOffset.mX + tEdge.mFrom.mX * tScale, tOffset.mY + tEdge.mFrom.mY * tScale};
            tScreen.mTo = {tOffset.mX + tEdge.mTo.mX * tScale, tOffset.mY + tEdge.mTo.mY * tScale};
            tScreen.mName = &tEdge.mName;
            tScreen.mRunway = tEdge.mRunway;
            tScreen.mOneWay = tEdge.mOneWay;
            tLines.addLine(tScreen.mFrom, tScreen.mTo, 1.5f, tEdge.mRunway ? kRunwayCrossing : kTaxiway);
            mEdges.push_back(tScreen);
        }
    }
    pContext.mCanvas->draw(tLines);
}

void TaxiRouteLayer::drawLabels(const drawContext& pContext)
{
    if (!pContext.mLabels)
    {
        return;
    }
    for (const ScreenEdge& tEdge : mEdges)
    {
        const float tLength = std::hypot(tEdge.mTo.mX - tEdge.mFrom.mX, tEdge.mTo.mY - tEdge.mFrom.mY);
        if (tEdge.mRunway || tEdge.mName->empty() || tLength < kMinLabelLength)
        {
            continue;
        }
        XMapSymbols::drawLabelCentered(*pContext.mLabels, (tEdge.mFrom.mX + tEdge.mTo.mX) * 0.5f,
                                      (tEdge.mFrom.mY + tEdge.mTo.mY) * 0.5f, *tEdge.mName, kLabel,
                                      pContext.labelShadow());
    }
}

std::string TaxiRouteLayer::describe(const drawContext& , double pX, double pY) const
{
    for (const ScreenEdge& tEdge : mEdges)
    {
        if (segmentDistance(tEdge.mFrom, tEdge.mTo, pX, pY) <= 3.0)
        {
            const std::string tName = tEdge.mName->empty() ? "(unnamed)" : *tEdge.mName;
            return std::string(tEdge.mRunway ? "Taxi route on runway " : "Taxi route ") + tName +
                   (tEdge.mOneWay ? ", one way" : ", two way");
        }
    }
    return "";
}
