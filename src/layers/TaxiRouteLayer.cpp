// TaxiRouteLayer.cpp
#include "layers/TaxiRouteLayer.h"

#include <cmath>

#include "LabelPlacer.h"
#include "MapView.h"
#include "data/AirportDetailStore.h"
#include "data/AirportGeometryStore.h"
#include "gfx/Canvas.h"
#include "layers/AirportVisibility.h"
#include "layers/MapSymbols.h"

namespace
{

const double kMinZoom = 14.0;
const float kMinLabelLength = 40.0f;  // bundan kisa kenara ad yazilmaz (boxel)
const Rgba kTaxiway = {0.85f, 0.45f, 1.00f, 0.75f};
const Rgba kRunwayCrossing = {1.00f, 0.35f, 0.35f, 0.75f};
const Rgba kLabel = {0.95f, 0.80f, 1.00f, 1.0f};

// Nokta - dogru parcasi uzakligi
double SegmentDistance(const PointF& pFrom, const PointF& pTo, double pX, double pY)
{
    const double tDx = pTo.mX - pFrom.mX;
    const double tDy = pTo.mY - pFrom.mY;
    const double tLength2 = tDx * tDx + tDy * tDy;
    double tT = tLength2 > 0.0 ? ((pX - pFrom.mX) * tDx + (pY - pFrom.mY) * tDy) / tLength2 : 0.0;
    tT = std::fmax(0.0, std::fmin(1.0, tT));
    return std::hypot(pFrom.mX + tT * tDx - pX, pFrom.mY + tT * tDy - pY);
}

}  // namespace

TaxiRouteLayer::TaxiRouteLayer(const AirportGeometryStore& pSummaries, AirportDetailStore& pDetails)
    : Layer("taxi_routes", "Taxi routes", false)
    , mSummaries(pSummaries)
    , mDetails(pDetails)
{
}

void TaxiRouteLayer::Draw(const DrawContext& pContext)
{
    mEdges.clear();
    const MapView& tView = *pContext.mView;
    if (tView.Zoom() < kMinZoom)
    {
        return;
    }
    FindVisibleAirports(tView, mSummaries, mQuery, mVisible);
    const float tScale = static_cast<float>(tView.WorldSize());
    Gfx::Mesh tLines;
    for (int tIndex : mVisible)
    {
        const LoadedAirport* tLoaded = mDetails.Use(tIndex);
        if (!tLoaded)
        {
            continue;
        }
        const AirportShapes& tShapes = tLoaded->mShapes;
        const PointF tOffset = tView.MercatorToScreen(tShapes.mOriginX, tShapes.mOriginY);
        for (const TaxiEdgeShape& tEdge : tShapes.mTaxiEdges)
        {
            ScreenEdge tScreen;
            tScreen.mFrom = {tOffset.mX + tEdge.mFrom.mX * tScale, tOffset.mY + tEdge.mFrom.mY * tScale};
            tScreen.mTo = {tOffset.mX + tEdge.mTo.mX * tScale, tOffset.mY + tEdge.mTo.mY * tScale};
            tScreen.mName = &tEdge.mName;
            tScreen.mRunway = tEdge.mRunway;
            tScreen.mOneWay = tEdge.mOneWay;
            tLines.AddLine(tScreen.mFrom, tScreen.mTo, 1.5f, tEdge.mRunway ? kRunwayCrossing : kTaxiway);
            mEdges.push_back(tScreen);
        }
    }
    pContext.mCanvas->Draw(tLines);
}

void TaxiRouteLayer::DrawLabels(const DrawContext& pContext)
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
        // Ayni adli komsu kenarlar ust uste binmesin: etiket yerlestirici eler
        MapSymbols::DrawLabelCentered(*pContext.mLabels, (tEdge.mFrom.mX + tEdge.mTo.mX) * 0.5f,
                                      (tEdge.mFrom.mY + tEdge.mTo.mY) * 0.5f, *tEdge.mName, kLabel,
                                      pContext.LabelShadow());
    }
}

std::string TaxiRouteLayer::Describe(const DrawContext& /*pContext*/, double pX, double pY) const
{
    for (const ScreenEdge& tEdge : mEdges)
    {
        if (SegmentDistance(tEdge.mFrom, tEdge.mTo, pX, pY) <= 3.0)
        {
            const std::string tName = tEdge.mName->empty() ? "(unnamed)" : *tEdge.mName;
            return std::string(tEdge.mRunway ? "Taxi route on runway " : "Taxi route ") + tName +
                   (tEdge.mOneWay ? ", one way" : ", two way");
        }
    }
    return "";
}
