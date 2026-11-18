// Harita kamerasi: merkez, zoom, gorunum alani ve Web Mercator donusumleri.
#ifndef PRIME_MAP_SRC_XMAPVIEW_H
#define PRIME_MAP_SRC_XMAPVIEW_H

#include "Geo.h"
#include "ScreenMath.h"

namespace Mercator
{
const double kMaxLat = 85.05112878;

double lonToX(double pLon);
double latToY(double pLat);
double xToLon(double pX);
double yToLat(double pY);
}

struct GeoBounds
{
    double mSouth = 0.0;
    double mWest = 0.0;
    double mNorth = 0.0;
    double mEast = 0.0;
};

class XMapView
{
public:
    static constexpr double kMinZoom = 3.0;
    static constexpr double kMaxZoom = 16.5;
    static constexpr double kHardMaxZoom = 18.0;
    static constexpr double kTileSize = 256.0;

    void setViewport(const RectD& pBoxels);
    const RectD& viewport() const;

    void setCenter(const LatLon& pCenter);
    LatLon center() const;
    double centerMercatorX() const;
    double centerMercatorY() const;
    void setCenterMercator(double pX, double pY);

    void setZoom(double pZoom);
    double zoom() const;
    void setZoomLimits(double pMinZoom, double pMaxZoom);
    double minZoomLimit() const;
    double maxZoomLimit() const;

    double worldSize() const;

    void latLonToBoxel(const LatLon& pPoint, double* pX, double* pY) const;
    PointF toScreen(const LatLon& pPoint) const;
    PointF mercatorToScreen(double pX, double pY) const;
    LatLon boxelToLatLon(double pX, double pY) const;

    void panByBoxels(double pDeltaX, double pDeltaY);

    void zoomAt(double pX, double pY, double pDelta);

    GeoBounds visibleBounds() const;

    GeoBounds boundsOf(const RectD& pBoxels) const;

    double metersPerBoxel() const;

    double metersPerBoxelAt(double pLat) const;

private:
    void clampCenter();

    RectD mViewport;
    double mCenterX = 0.5;
    double mCenterY = 0.5;
    double mZoom = 6.0;
    double mMinZoom = kMinZoom;
    double mMaxZoom = kMaxZoom;
};
#endif
