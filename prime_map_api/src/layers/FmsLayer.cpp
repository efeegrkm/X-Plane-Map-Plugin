#include "layers/FmsLayer.h"

#include <algorithm>
#include <array>
#include <format>

#include "XPLMNavigation.h"

#include "LabelPlacer.h"
#include "XMapView.h"
#include "gfx/Canvas.h"
#include "ui/UiDraw.h"

namespace
{
const float kReadInterval = 1.0f;
const double kDensifyNm = 5.0;
const double kLabelMinZoom = 5.0;
const float kWaypointRadius = 5.0f;

const Rgba kLegColor = {0.90f, 0.25f, 0.90f, 1.0f};
const Rgba kPastLegColor = {0.60f, 0.30f, 0.60f, 0.75f};
const Rgba kActiveLegColor = {1.0f, 0.55f, 1.0f, 1.0f};
const Rgba kLabelColor = {1.0f, 0.75f, 1.0f, 1.0f};

Gfx::Point project(const XMapView& pView, const LatLon& pPoint)
{
    return pView.toScreen(pPoint);
}
}

FmsLayer::FmsLayer()
    : Layer("fms", "FMS route", true)
{
}

void FmsLayer::setFromHost(bool pFromHost)
{
    mFromHost = pFromHost;
    mWaypoints.clear();
    mDestination = -1;
    mLastReadTime = -10.0f;
}

bool FmsLayer::isFromHost() const
{
    return mFromHost;
}

void FmsLayer::setHostRoute(const std::vector<Waypoint>& pWaypoints, int pActive)
{
    if (!mFromHost)
    {
        return;
    }
    mWaypoints = pWaypoints;
    mDestination = (pActive >= 0 && pActive < static_cast<int>(mWaypoints.size())) ? pActive : -1;
}

void FmsLayer::update(float pNow)
{
    if (mFromHost)
    {
        return;
    }
    if (pNow - mLastReadTime < kReadInterval)
    {
        return;
    }
    mLastReadTime = pNow;
    readFms();
}

void FmsLayer::readFms()
{
    mWaypoints.clear();
    const int tCount = XPLMCountFMSEntries();
    for (int tIndex = 0; tIndex < tCount; ++tIndex)
    {
        XPLMNavType tType = xplm_Nav_Unknown;
        std::array<char, 256> tId = {};
        XPLMNavRef tRef = XPLM_NAV_NOT_FOUND;
        int tAltitude = 0;
        float tLat = 0.0f;
        float tLon = 0.0f;
        XPLMGetFMSEntryInfo(tIndex, &tType, tId.data(), &tRef, &tAltitude, &tLat, &tLon);
        if (tLat == 0.0f && tLon == 0.0f && tRef != XPLM_NAV_NOT_FOUND)
        {
            XPLMGetNavAidInfo(tRef, nullptr, &tLat, &tLon, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        }
        if (tLat == 0.0f && tLon == 0.0f)
        {
            continue;
        }
        mWaypoints.push_back({{tLat, tLon}, tId.data(), tAltitude});
    }
    mDestination = (tCount > 0) ? XPLMGetDestinationFMSEntry() : -1;
}

std::vector<LatLon> FmsLayer::routeAhead() const
{
    std::vector<LatLon> tPoints;
    for (size_t tIndex = static_cast<size_t>(std::max(0, mDestination)); tIndex < mWaypoints.size(); ++tIndex)
    {
        tPoints.push_back(mWaypoints[tIndex].mPosition);
    }
    return tPoints;
}

void FmsLayer::draw(const drawContext& pContext)
{
    mLabels.clear();
    if (mWaypoints.empty())
    {
        return;
    }
    const XMapView& tView = *pContext.mView;

    Gfx::OutlinedLines tLegs;
    Gfx::OutlinedLines tActive;
    for (size_t tIndex = 0; tIndex + 1 < mWaypoints.size(); ++tIndex)
    {
        std::vector<LatLon> tDense;
        densifySegment(mWaypoints[tIndex].mPosition, mWaypoints[tIndex + 1].mPosition, kDensifyNm, tDense);
        tDense.push_back(mWaypoints[tIndex + 1].mPosition);
        std::vector<Gfx::Point> tScreen;
        for (const LatLon& tPoint : tDense)
        {
            tScreen.push_back(project(tView, tPoint));
        }
        const int tLegEnd = static_cast<int>(tIndex) + 1;
        if (tLegEnd == mDestination)
        {
            tActive.addPolyline(tScreen, false, 4.0f, kActiveLegColor);
        }
        else
        {
            tLegs.addPolyline(tScreen, false, 2.5f, tLegEnd < mDestination ? kPastLegColor : kLegColor);
        }
    }
    Gfx::Canvas& tCanvas = *pContext.mCanvas;
    tCanvas.draw(tLegs);
    tCanvas.draw(tActive);

    Gfx::OutlinedLines tDiamonds;
    const bool tWithLabels = tView.zoom() >= kLabelMinZoom;
    for (size_t tIndex = 0; tIndex < mWaypoints.size(); ++tIndex)
    {
        const Waypoint& tWaypoint = mWaypoints[tIndex];
        const Gfx::Point tPoint = project(tView, tWaypoint.mPosition);
        const bool tIsDestination = static_cast<int>(tIndex) == mDestination;
        tDiamonds.addRegularPolygon(tPoint, kWaypointRadius, 4, 90.0f, 1.5f,
                                    tIsDestination ? kActiveLegColor : kLegColor);
        if (!tWithLabels)
        {
            continue;
        }
        PendingLabel tLabel;
        tLabel.mText = tWaypoint.mAltitudeFt > 0 ? std::format("{}  {} ft", tWaypoint.mId, tWaypoint.mAltitudeFt)
                                                 : tWaypoint.mId;
        tLabel.mX = static_cast<int>(tPoint.mX) + 9;
        tLabel.mY = static_cast<int>(tPoint.mY) + 4;
        if (pContext.mLabels)
        {
            RectD tBox;
            tBox.mLeft = tLabel.mX - 1;
            tBox.mRight = tLabel.mX + Ui::textWidth(tLabel.mText) + 2;
            tBox.mBottom = tLabel.mY - 3;
            tBox.mTop = tLabel.mY + Ui::fontHeight() + 1;
            pContext.mLabels->reserve(tBox);
        }
        mLabels.push_back(tLabel);
    }
    tCanvas.draw(tDiamonds);
}

void FmsLayer::drawLabels(const drawContext& )
{
    for (const PendingLabel& tLabel : mLabels)
    {
        Ui::textShadowed(tLabel.mX, tLabel.mY, tLabel.mText, kLabelColor);
    }
}
