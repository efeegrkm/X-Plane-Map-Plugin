// AirportLayer.cpp
#include "layers/AirportLayer.h"

#include <cstdio>

#include "GLInclude.h"
#include "MapView.h"
#include "data/NavDatabase.h"
#include "layers/MapSymbols.h"

namespace
{

const double kMinZoom = 4.0;
const double kMinorMinZoom = 8.0;
const double kLabelMinZoom = 6.0;
const double kMinorLabelMinZoom = 9.0;
const double kNameMinZoom = 11.0;
const size_t kMaxSymbols = 3000;   // FPS korumasi: bir karede en fazla bu kadar sembol
const double kEdgeMargin = 20.0;   // kenardan tasan semboller de cizilsin (boxel)

const float kMajorRadius = 6.0f;
const float kMinorRadius = 4.0f;

const Rgba kMajorColor = {0.40f, 0.72f, 1.00f, 1.0f};
const Rgba kMinorColor = {0.55f, 0.65f, 0.78f, 1.0f};
const Rgba kLabelColor = {0.80f, 0.90f, 1.00f, 1.0f};
const Rgba kMinorLabelColor = {0.66f, 0.73f, 0.82f, 1.0f};

}  // namespace

AirportLayer::AirportLayer(const NavDatabase& pDatabase)
    : Layer("havalimani", "Havalimanlari", true)
    , mDatabase(pDatabase)
{
}

void AirportLayer::CollectVisible(const DrawContext& pContext)
{
    mVisible.clear();
    const MapView& tView = *pContext.mView;
    if (!mDatabase.IsReady() || tView.Zoom() < kMinZoom)
    {
        return;
    }

    RectD tArea = pContext.mArea;
    tArea.mLeft -= kEdgeMargin;
    tArea.mRight += kEdgeMargin;
    tArea.mBottom -= kEdgeMargin;
    tArea.mTop += kEdgeMargin;

    mQuery.clear();
    mDatabase.Query(NavKind::Airport, tView.BoundsOf(tArea), mQuery);
    mSymbolPlacer.Reset(tArea);

    // 1. gecis: buyuk havalimanlari, 2. gecis: digerleri (yer kalirsa)
    const bool tShowMinor = tView.Zoom() >= kMinorMinZoom;
    for (int tPass = 0; tPass < 2; ++tPass)
    {
        const bool tWantMajor = (tPass == 0);
        if (!tWantMajor && !tShowMinor)
        {
            break;
        }
        for (int tIndex : mQuery)
        {
            const NavRecord& tRecord = mDatabase.At(tIndex);
            if (tRecord.mMajor != tWantMajor)
            {
                continue;
            }
            double tX = 0.0;
            double tY = 0.0;
            tView.LatLonToBoxel({tRecord.mLat, tRecord.mLon}, &tX, &tY);
            if (!tArea.Contains(tX, tY))
            {
                continue;
            }
            const double tRadius = (tWantMajor ? kMajorRadius : kMinorRadius) + 1.0;
            RectD tBox;
            tBox.mLeft = tX - tRadius;
            tBox.mRight = tX + tRadius;
            tBox.mBottom = tY - tRadius;
            tBox.mTop = tY + tRadius;
            if (!mSymbolPlacer.TryPlace(tBox))
            {
                continue;  // baska bir havalimaninin sembolunun ustune biniyor
            }
            mVisible.push_back({tIndex, static_cast<float>(tX), static_cast<float>(tY), tWantMajor});
            if (mVisible.size() >= kMaxSymbols)
            {
                return;
            }
        }
    }
}

void AirportLayer::Draw(const DrawContext& pContext)
{
    CollectVisible(pContext);
    if (mVisible.empty())
    {
        return;
    }
    Ui::UseFlatState();

    // Koyu kontur, sonra renkli halka
    for (int tPass = 0; tPass < 2; ++tPass)
    {
        const bool tOutline = (tPass == 0);
        glLineWidth(tOutline ? MapSymbols::kOutlineWidth : MapSymbols::kLineWidth);
        MapSymbols::SetColor(MapSymbols::kOutline);
        glBegin(GL_LINES);
        for (const VisibleAirport& tAirport : mVisible)
        {
            if (!tOutline)
            {
                MapSymbols::SetColor(tAirport.mMajor ? kMajorColor : kMinorColor);
            }
            MapSymbols::EmitPolygonEdges(tAirport.mX, tAirport.mY, tAirport.mMajor ? kMajorRadius : kMinorRadius,
                                         16, 0.0f);
        }
        glEnd();
    }

    // Buyuklerin ortasinda dolu nokta
    MapSymbols::SetColor(kMajorColor);
    glBegin(GL_TRIANGLES);
    for (const VisibleAirport& tAirport : mVisible)
    {
        if (tAirport.mMajor)
        {
            MapSymbols::EmitDisc(tAirport.mX, tAirport.mY, 2.0f, 8);
        }
    }
    glEnd();
    glLineWidth(1.0f);
}

void AirportLayer::DrawLabels(const DrawContext& pContext)
{
    const double tZoom = pContext.mView->Zoom();
    if (!pContext.mLabels || tZoom < kLabelMinZoom)
    {
        return;
    }
    char tText[128];
    for (const VisibleAirport& tAirport : mVisible)
    {
        if (!tAirport.mMajor && tZoom < kMinorLabelMinZoom)
        {
            continue;
        }
        const NavRecord& tRecord = mDatabase.At(tAirport.mIndex);
        if (tZoom >= kNameMinZoom && !tRecord.mName.empty())
        {
            std::snprintf(tText, sizeof(tText), "%s  %s", tRecord.mId.c_str(), tRecord.mName.c_str());
        }
        else
        {
            std::snprintf(tText, sizeof(tText), "%s", tRecord.mId.c_str());
        }
        const float tRadius = tAirport.mMajor ? kMajorRadius : kMinorRadius;
        MapSymbols::DrawLabelNear(*pContext.mLabels, tAirport.mX, tAirport.mY, tRadius + 4.0f, tText,
                                  tAirport.mMajor ? kLabelColor : kMinorLabelColor);
    }
}
