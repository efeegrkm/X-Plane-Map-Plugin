// FmsLayer.cpp
#include "layers/FmsLayer.h"

#include <cstdio>

#include "XPLMNavigation.h"

#include "LabelPlacer.h"
#include "MapView.h"

namespace
{

const float kReadInterval = 1.0f;     // saniye
const double kDensifyNm = 5.0;
const double kLabelMinZoom = 5.0;
const float kWaypointRadius = 5.0f;

const Rgba kLegColor = {0.90f, 0.25f, 0.90f, 1.0f};
const Rgba kPastLegColor = {0.60f, 0.30f, 0.60f, 0.75f};
const Rgba kActiveLegColor = {1.0f, 0.55f, 1.0f, 1.0f};
const Rgba kLabelColor = {1.0f, 0.75f, 1.0f, 1.0f};

Gfx::Point Project(const MapView& pView, const LatLon& pPoint)
{
    double tX = 0.0;
    double tY = 0.0;
    pView.LatLonToBoxel(pPoint, &tX, &tY);
    return {static_cast<float>(tX), static_cast<float>(tY)};
}

}  // namespace

FmsLayer::FmsLayer()
    : Layer("fms", "FMS route", true)
{
}

void FmsLayer::Update(float pNow)
{
    if (pNow - mLastReadTime < kReadInterval)
    {
        return;
    }
    mLastReadTime = pNow;
    ReadFms();
}

void FmsLayer::ReadFms()
{
    mWaypoints.clear();
    const int tCount = XPLMCountFMSEntries();
    for (int tIndex = 0; tIndex < tCount; ++tIndex)
    {
        XPLMNavType tType = xplm_Nav_Unknown;
        char tId[256] = "";
        XPLMNavRef tRef = XPLM_NAV_NOT_FOUND;  // SDK uyarisi: once NOT_FOUND ile baslat
        int tAltitude = 0;
        float tLat = 0.0f;
        float tLon = 0.0f;
        XPLMGetFMSEntryInfo(tIndex, &tType, tId, &tRef, &tAltitude, &tLat, &tLon);
        if (tLat == 0.0f && tLon == 0.0f && tRef != XPLM_NAV_NOT_FOUND)
        {
            // Konum gelmediyse nav kaydindan al
            XPLMGetNavAidInfo(tRef, nullptr, &tLat, &tLon, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        }
        if (tLat == 0.0f && tLon == 0.0f)
        {
            continue;  // bos veya henuz cozulmemis satir
        }
        mWaypoints.push_back({{tLat, tLon}, tId, tAltitude});
    }
    mDestination = (tCount > 0) ? XPLMGetDestinationFMSEntry() : -1;
}

void FmsLayer::Draw(const DrawContext& pContext)
{
    mLabels.clear();
    if (mWaypoints.empty())
    {
        return;
    }
    const MapView& tView = *pContext.mView;

    Gfx::LineBatch tLegs;
    Gfx::LineBatch tActive;
    for (size_t tIndex = 0; tIndex + 1 < mWaypoints.size(); ++tIndex)
    {
        std::vector<LatLon> tDense;
        DensifySegment(mWaypoints[tIndex].mPosition, mWaypoints[tIndex + 1].mPosition, kDensifyNm, tDense);
        tDense.push_back(mWaypoints[tIndex + 1].mPosition);
        std::vector<Gfx::Point> tScreen;
        for (const LatLon& tPoint : tDense)
        {
            tScreen.push_back(Project(tView, tPoint));
        }
        const int tLegEnd = static_cast<int>(tIndex) + 1;
        if (tLegEnd == mDestination)
        {
            tActive.AddPolyline(tScreen, false, kActiveLegColor);
        }
        else
        {
            tLegs.AddPolyline(tScreen, false, tLegEnd < mDestination ? kPastLegColor : kLegColor);
        }
    }
    tLegs.DrawOutlined(2.5f);
    tActive.DrawOutlined(4.0f);

    // Noktalar: elmas (45 derece donuk kare)
    Gfx::LineBatch tDiamonds;
    const bool tWithLabels = tView.Zoom() >= kLabelMinZoom;
    char tText[300];
    for (size_t tIndex = 0; tIndex < mWaypoints.size(); ++tIndex)
    {
        const Waypoint& tWaypoint = mWaypoints[tIndex];
        const Gfx::Point tPoint = Project(tView, tWaypoint.mPosition);
        const bool tIsDestination = static_cast<int>(tIndex) == mDestination;
        tDiamonds.AddRegularPolygon(tPoint, kWaypointRadius, 4, 90.0f, tIsDestination ? kActiveLegColor : kLegColor);
        if (!tWithLabels)
        {
            continue;
        }
        if (tWaypoint.mAltitudeFt > 0)
        {
            std::snprintf(tText, sizeof(tText), "%s  %d ft", tWaypoint.mId.c_str(), tWaypoint.mAltitudeFt);
        }
        else
        {
            std::snprintf(tText, sizeof(tText), "%s", tWaypoint.mId.c_str());
        }
        PendingLabel tLabel;
        tLabel.mText = tText;
        tLabel.mX = static_cast<int>(tPoint.mX) + 9;
        tLabel.mY = static_cast<int>(tPoint.mY) + 4;
        if (pContext.mLabels)
        {
            RectD tBox;
            tBox.mLeft = tLabel.mX - 1;
            tBox.mRight = tLabel.mX + Ui::TextWidth(tText) + 2;
            tBox.mBottom = tLabel.mY - 3;
            tBox.mTop = tLabel.mY + Ui::FontHeight() + 1;
            pContext.mLabels->Reserve(tBox);  // navaid etiketleri bunun ustune yazmasin
        }
        mLabels.push_back(tLabel);
    }
    tDiamonds.DrawOutlined(1.5f);
}

void FmsLayer::DrawLabels(const DrawContext& /*pContext*/)
{
    for (const PendingLabel& tLabel : mLabels)
    {
        Ui::TextShadowed(tLabel.mX, tLabel.mY, tLabel.mText.c_str(), kLabelColor);
    }
}
