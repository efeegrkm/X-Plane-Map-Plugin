// RunwayLayer.cpp
#include "layers/RunwayLayer.h"

#include <cmath>
#include <format>

#include "Geo.h"
#include "MapView.h"
#include "data/AirportGeometryStore.h"
#include "gfx/Canvas.h"
#include "layers/MapSymbols.h"

namespace
{

const double kMinZoom = 9.0;
const double kLabelMinZoom = 11.5;
const double kOverrunMinZoom = 12.0;
const double kMarkingMinZoom = 13.0;
const double kQueryMarginDeg = 0.1;   // havalimani merkezinden pist ucuna ~ birkac km
const double kEdgeMargin = 40.0;      // boxel
const float kMinHalfWidth = 1.0f;     // cok uzaktan bakinca bile pist 2 boxel gorunsun
const float kMinLength = 3.0f;        // bundan kisa pist cizilmez (havalimani simgesi yeter)
const float kLabelGap = 10.0f;        // pist numarasinin uctan uzakligi (boxel)
const double kDashMeters = 30.0;      // orta hat kesik uzunlugu (gercek pistlerde ~36 m)

const Rgba kPavedColor = {0.22f, 0.23f, 0.26f, 1.0f};    // koyu asfalt: zemin renklerinden ayrilsin
const Rgba kUnpavedColor = {0.52f, 0.44f, 0.30f, 1.0f};  // toprak/cim pist: kahverengimsi
const Rgba kWaterColor = {0.30f, 0.50f, 0.80f, 0.85f};
const Rgba kSnowColor = {0.85f, 0.88f, 0.92f, 1.0f};
const Rgba kOverrunColor = {0.33f, 0.34f, 0.37f, 1.0f};
const Rgba kMarkingColor = {0.96f, 0.97f, 0.98f, 1.0f};
const Rgba kEdgeColor = {0.90f, 0.92f, 0.95f, 0.90f};    // acik kenar: hem zeminde hem koyu arka planda okunur
const Rgba kLabelColor = {0.92f, 0.94f, 0.97f, 1.0f};

const Rgba& SurfaceColor(RunwaySurface pSurface)
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

}  // namespace

RunwayLayer::RunwayLayer(const AirportGeometryStore& pStore)
    : Layer("runways", "Runways", true)
    , mStore(pStore)
{
}

void RunwayLayer::CollectVisible(const DrawContext& pContext)
{
    mVisible.clear();
    const MapView& tView = *pContext.mView;
    if (!mStore.IsReady() || tView.Zoom() < kMinZoom)
    {
        return;
    }

    RectD tArea = pContext.mArea;
    tArea.mLeft -= kEdgeMargin;
    tArea.mRight += kEdgeMargin;
    tArea.mBottom -= kEdgeMargin;
    tArea.mTop += kEdgeMargin;
    GeoBounds tBounds = tView.BoundsOf(tArea);
    tBounds.mSouth -= kQueryMarginDeg;
    tBounds.mNorth += kQueryMarginDeg;
    tBounds.mWest -= kQueryMarginDeg;
    tBounds.mEast += kQueryMarginDeg;

    mQuery.clear();
    mStore.Query(tBounds, mQuery);

    for (int tAirportIndex : mQuery)
    {
        for (const Runway& tRunway : mStore.At(tAirportIndex).mRunways)
        {
            ScreenRunway tScreen;
            tScreen.mRunway = &tRunway;
            for (int tEnd = 0; tEnd < 2; ++tEnd)
            {
                const PointF tPoint = tView.ToScreen(tRunway.mEnds[tEnd].mPosition);
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

            // Ekranda gorunuyor mu (pistin kapsayan kutusu alana degiyor mu)
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
            tScreen.mBoxelsPerMeter = static_cast<float>(1.0 / tView.MetersPerBoxelAt(tMidLat));
            tScreen.mHalfWidth =
                std::fmax(static_cast<float>(tRunway.mWidthM) * tScreen.mBoxelsPerMeter * 0.5f, kMinHalfWidth);
            mVisible.push_back(tScreen);
        }
    }
}

void RunwayLayer::AddRectangle(Gfx::Mesh& pMesh, const ScreenRunway& pRunway, float pFrom, float pTo,
                               float pHalfWidth, const Rgba& pColor)
{
    // Normal: pist yonunun 90 derece dondurulmusu
    const float tNormalX = -pRunway.mDirY * pHalfWidth;
    const float tNormalY = pRunway.mDirX * pHalfWidth;
    const float tStartX = pRunway.mEndX[0] + pRunway.mDirX * pFrom;
    const float tStartY = pRunway.mEndY[0] + pRunway.mDirY * pFrom;
    const float tStopX = pRunway.mEndX[0] + pRunway.mDirX * pTo;
    const float tStopY = pRunway.mEndY[0] + pRunway.mDirY * pTo;
    pMesh.AddQuad({tStartX + tNormalX, tStartY + tNormalY}, {tStartX - tNormalX, tStartY - tNormalY},
                  {tStopX - tNormalX, tStopY - tNormalY}, {tStopX + tNormalX, tStopY + tNormalY}, pColor);
}

void RunwayLayer::Draw(const DrawContext& pContext)
{
    CollectVisible(pContext);
    if (mVisible.empty())
    {
        return;
    }
    const double tZoom = pContext.mView->Zoom();
    Gfx::Mesh tMesh;

    // Blast pad / overrun: pistin iki ucundan disari
    if (tZoom >= kOverrunMinZoom)
    {
        for (const ScreenRunway& tRunway : mVisible)
        {
            const float tOverrun0 = static_cast<float>(tRunway.mRunway->mEnds[0].mOverrunM) * tRunway.mBoxelsPerMeter;
            const float tOverrun1 = static_cast<float>(tRunway.mRunway->mEnds[1].mOverrunM) * tRunway.mBoxelsPerMeter;
            if (tOverrun0 > 0.5f)
            {
                AddRectangle(tMesh, tRunway, -tOverrun0, 0.0f, tRunway.mHalfWidth, kOverrunColor);
            }
            if (tOverrun1 > 0.5f)
            {
                AddRectangle(tMesh, tRunway, tRunway.mLength, tRunway.mLength + tOverrun1, tRunway.mHalfWidth,
                             kOverrunColor);
            }
        }
    }

    // Once TUM pistlerin acik kenari, sonra TUM yuzeyler: kesisen pistlerde
    // birinin kenari digerinin yuzeyini cizmesin
    for (const ScreenRunway& tRunway : mVisible)
    {
        if (tRunway.mHalfWidth >= 2.0f)
        {
            AddRectangle(tMesh, tRunway, -1.0f, tRunway.mLength + 1.0f, tRunway.mHalfWidth + 1.0f, kEdgeColor);
        }
    }
    for (const ScreenRunway& tRunway : mVisible)
    {
        if (tRunway.mRunway->mSurface != RunwaySurface::Transparent)
        {
            // Uzaktan (cok dar) pist: koyu dolgu gorunmez, acik renkli cizgi olarak ciz
            const bool tThin = tRunway.mHalfWidth < 2.0f;
            AddRectangle(tMesh, tRunway, 0.0f, tRunway.mLength, tRunway.mHalfWidth,
                         tThin ? kEdgeColor : SurfaceColor(tRunway.mRunway->mSurface));
        }
    }

    if (tZoom >= kMarkingMinZoom)
    {
        for (const ScreenRunway& tRunway : mVisible)
        {
            if (tRunway.mRunway->mSurface == RunwaySurface::Paved && tRunway.mHalfWidth >= 3.0f)
            {
                AddMarkings(tMesh, tRunway);
            }
        }
    }
    pContext.mCanvas->Draw(tMesh);
}

void RunwayLayer::AddMarkings(Gfx::Mesh& pMesh, const ScreenRunway& pRunway)
{
    // Esikler: kaydirilmis esik varsa uctan o kadar iceride
    const float tThreshold0 = static_cast<float>(pRunway.mRunway->mEnds[0].mDisplacedThresholdM) *
                              pRunway.mBoxelsPerMeter;
    const float tThreshold1 = pRunway.mLength - static_cast<float>(pRunway.mRunway->mEnds[1].mDisplacedThresholdM) *
                                                    pRunway.mBoxelsPerMeter;
    const float tBar = 1.5f;  // esik cizgisinin kalinligi (boxel)
    AddRectangle(pMesh, pRunway, tThreshold0, tThreshold0 + tBar, pRunway.mHalfWidth * 0.85f, kMarkingColor);
    AddRectangle(pMesh, pRunway, tThreshold1 - tBar, tThreshold1, pRunway.mHalfWidth * 0.85f, kMarkingColor);

    // Orta hat: esikler arasinda kesikli (bosluk = kesik boyu)
    const float tDash = std::fmax(static_cast<float>(kDashMeters) * pRunway.mBoxelsPerMeter, 3.0f);
    const float tLineHalf = std::fmax(pRunway.mHalfWidth * 0.03f, 0.5f);
    for (float tAt = tThreshold0 + tDash; tAt + tDash < tThreshold1 - tDash; tAt += 2.0f * tDash)
    {
        AddRectangle(pMesh, pRunway, tAt, tAt + tDash, tLineHalf, kMarkingColor);
    }
}

void RunwayLayer::DrawLabels(const DrawContext& pContext)
{
    if (!pContext.mLabels || pContext.mView->Zoom() < kLabelMinZoom)
    {
        return;
    }
    for (const ScreenRunway& tRunway : mVisible)
    {
        // Uc 0'in numarasi pistin uc 0 tarafinin disinda, uc 1'inki obur tarafta
        for (int tEnd = 0; tEnd < 2; ++tEnd)
        {
            const float tSign = (tEnd == 0) ? -1.0f : 1.0f;
            const float tOverrun =
                static_cast<float>(tRunway.mRunway->mEnds[tEnd].mOverrunM) * tRunway.mBoxelsPerMeter;
            const float tOffset = kLabelGap + tOverrun;
            const float tX = tRunway.mEndX[tEnd] + tSign * tRunway.mDirX * tOffset;
            const float tY = tRunway.mEndY[tEnd] + tSign * tRunway.mDirY * tOffset;
            MapSymbols::DrawLabelCentered(*pContext.mLabels, tX, tY, tRunway.mRunway->mEnds[tEnd].mName,
                                          kLabelColor);
        }
    }
}

std::string RunwayLayer::Describe(const DrawContext& /*pContext*/, double pX, double pY) const
{
    for (const ScreenRunway& tRunway : mVisible)
    {
        // Imlec, pist ekseni koordinatinda: boyuna (0..boy) ve enine (yari genislik)
        const float tDeltaX = static_cast<float>(pX) - tRunway.mEndX[0];
        const float tDeltaY = static_cast<float>(pY) - tRunway.mEndY[0];
        const float tAlong = tDeltaX * tRunway.mDirX + tDeltaY * tRunway.mDirY;
        const float tAcross = std::fabs(-tDeltaX * tRunway.mDirY + tDeltaY * tRunway.mDirX);
        if (tAlong < 0.0f || tAlong > tRunway.mLength || tAcross > tRunway.mHalfWidth + 2.0f)
        {
            continue;
        }
        const Runway& tData = *tRunway.mRunway;
        const double tLengthM = DistanceNm(tData.mEnds[0].mPosition, tData.mEnds[1].mPosition) * 1852.0;
        return std::format("{} {}/{}  {:.0f} x {:.0f} m, {}", tData.mWater ? "Water runway" : "Runway",
                           tData.mEnds[0].mName, tData.mEnds[1].mName, tLengthM, tData.mWidthM,
                           SurfaceName(tData.mSurfaceCode));
    }
    return "";
}
