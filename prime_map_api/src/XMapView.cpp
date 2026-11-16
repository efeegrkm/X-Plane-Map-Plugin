#include "XMapView.h"

#include <cmath>

namespace
{

const double kPi = 3.14159265358979323846;
const double kEarthRadiusM = 6378137.0;  // Web Mercator'un kullandigi dunya yaricapi

double clamp(double pValue, double pLow, double pHigh)
{
    if (pValue < pLow)
    {
        return pLow;
    }
    if (pValue > pHigh)
    {
        return pHigh;
    }
    return pValue;
}

// x'i 0..1 araligina sar (dunya dogu-bati yonunde tekrar eder)
double wrapX(double pX)
{
    return pX - std::floor(pX);
}

}  // namespace

namespace Mercator
{

double lonToX(double pLon)
{
    return (pLon + 180.0) / 360.0;
}

double latToY(double pLat)
{
    const double tLat = clamp(pLat, -kMaxLat, kMaxLat) * kPi / 180.0;
    // kMaxLat yuvarlanmis bir sayi: sonuc 1'i cok az asabilir, kirpilir
    return clamp(0.5 + std::log(std::tan(kPi / 4.0 + tLat / 2.0)) / (2.0 * kPi), 0.0, 1.0);
}

double xToLon(double pX)
{
    return pX * 360.0 - 180.0;
}

double yToLat(double pY)
{
    const double tMercator = (pY - 0.5) * 2.0 * kPi;
    return (2.0 * std::atan(std::exp(tMercator)) - kPi / 2.0) * 180.0 / kPi;
}

}  // namespace Mercator

void XMapView::setViewport(const RectD& pBoxels)
{
    mViewport = pBoxels;
}

const RectD& XMapView::viewport() const
{
    return mViewport;
}

void XMapView::setCenter(const LatLon& pCenter)
{
    mCenterX = Mercator::lonToX(pCenter.mLon);
    mCenterY = Mercator::latToY(pCenter.mLat);
    clampCenter();
}

double XMapView::centerMercatorX() const
{
    return mCenterX;
}

double XMapView::centerMercatorY() const
{
    return mCenterY;
}

void XMapView::setCenterMercator(double pX, double pY)
{
    mCenterX = pX - std::floor(pX);  // 180. boylamda sarilma
    mCenterY = pY;
    clampCenter();
}

LatLon XMapView::center() const
{
    LatLon tCenter;
    tCenter.mLat = Mercator::yToLat(mCenterY);
    tCenter.mLon = Mercator::xToLon(mCenterX);
    return tCenter;
}

void XMapView::setZoom(double pZoom)
{
    mZoom = clamp(pZoom, mMinZoom, mMaxZoom);
}

void XMapView::setZoomLimits(double pMinZoom, double pMaxZoom)
{
    mMinZoom = clamp(pMinZoom, kMinZoom, kHardMaxZoom);
    mMaxZoom = clamp(pMaxZoom, mMinZoom, kHardMaxZoom);
    setZoom(mZoom);
}

double XMapView::minZoomLimit() const
{
    return mMinZoom;
}

double XMapView::maxZoomLimit() const
{
    return mMaxZoom;
}

double XMapView::zoom() const
{
    return mZoom;
}

double XMapView::worldSize() const
{
    return kTileSize * std::pow(2.0, mZoom);
}

void XMapView::latLonToBoxel(const LatLon& pPoint, double* pX, double* pY) const
{
    const double tWorld = worldSize();
    double tDeltaX = Mercator::lonToX(pPoint.mLon) - mCenterX;
    if (tDeltaX > 0.5)
    {
        tDeltaX -= 1.0;
    }
    else if (tDeltaX < -0.5)
    {
        tDeltaX += 1.0;
    }
    const double tDeltaY = Mercator::latToY(pPoint.mLat) - mCenterY;

    const double tMidX = (mViewport.mLeft + mViewport.mRight) * 0.5;
    const double tMidY = (mViewport.mBottom + mViewport.mTop) * 0.5;
    *pX = tMidX + tDeltaX * tWorld;
    *pY = tMidY + tDeltaY * tWorld;
}

PointF XMapView::mercatorToScreen(double pX, double pY) const
{
    double tDeltaX = pX - mCenterX;
    if (tDeltaX > 0.5)
    {
        tDeltaX -= 1.0;
    }
    else if (tDeltaX < -0.5)
    {
        tDeltaX += 1.0;
    }
    const double tWorld = worldSize();
    const double tMidX = (mViewport.mLeft + mViewport.mRight) * 0.5;
    const double tMidY = (mViewport.mBottom + mViewport.mTop) * 0.5;
    return {static_cast<float>(tMidX + tDeltaX * tWorld), static_cast<float>(tMidY + (pY - mCenterY) * tWorld)};
}

PointF XMapView::toScreen(const LatLon& pPoint) const
{
    double tX = 0.0;
    double tY = 0.0;
    latLonToBoxel(pPoint, &tX, &tY);
    return {static_cast<float>(tX), static_cast<float>(tY)};
}

LatLon XMapView::boxelToLatLon(double pX, double pY) const
{
    const double tWorld = worldSize();
    const double tMidX = (mViewport.mLeft + mViewport.mRight) * 0.5;
    const double tMidY = (mViewport.mBottom + mViewport.mTop) * 0.5;
    const double tMercX = wrapX(mCenterX + (pX - tMidX) / tWorld);
    const double tMercY = clamp(mCenterY + (pY - tMidY) / tWorld, 0.0, 1.0);

    LatLon tResult;
    tResult.mLat = Mercator::yToLat(tMercY);
    tResult.mLon = Mercator::xToLon(tMercX);
    return tResult;
}

void XMapView::panByBoxels(double pDeltaX, double pDeltaY)
{
    // Icerik fareyle birlikte hareket etsin diye merkez ters yone kayar
    const double tWorld = worldSize();
    mCenterX -= pDeltaX / tWorld;
    mCenterY -= pDeltaY / tWorld;
    clampCenter();
}

void XMapView::zoomAt(double pX, double pY, double pDelta)
{
    const double tMidX = (mViewport.mLeft + mViewport.mRight) * 0.5;
    const double tMidY = (mViewport.mBottom + mViewport.mTop) * 0.5;

    // Imlecin altindaki nokta (Mercator birimi), zoom'dan once
    const double tOldWorld = worldSize();
    const double tPointX = mCenterX + (pX - tMidX) / tOldWorld;
    const double tPointY = mCenterY + (pY - tMidY) / tOldWorld;

    setZoom(mZoom + pDelta);

    // Ayni nokta yine imlecin altina dussun diye merkezi yeniden hesapla
    const double tNewWorld = worldSize();
    mCenterX = tPointX - (pX - tMidX) / tNewWorld;
    mCenterY = tPointY - (pY - tMidY) / tNewWorld;
    clampCenter();
}

GeoBounds XMapView::visibleBounds() const
{
    return boundsOf(mViewport);
}

GeoBounds XMapView::boundsOf(const RectD& pBoxels) const
{
    const double tWorld = worldSize();
    const double tMidX = (mViewport.mLeft + mViewport.mRight) * 0.5;
    const double tMidY = (mViewport.mBottom + mViewport.mTop) * 0.5;

    // Bati/dogu sarilmaz (WrapX yok): 180. boylami gecen alan -180..180 disina tasar
    GeoBounds tBounds;
    tBounds.mWest = Mercator::xToLon(mCenterX + (pBoxels.mLeft - tMidX) / tWorld);
    tBounds.mEast = Mercator::xToLon(mCenterX + (pBoxels.mRight - tMidX) / tWorld);
    tBounds.mSouth = Mercator::yToLat(clamp(mCenterY + (pBoxels.mBottom - tMidY) / tWorld, 0.0, 1.0));
    tBounds.mNorth = Mercator::yToLat(clamp(mCenterY + (pBoxels.mTop - tMidY) / tWorld, 0.0, 1.0));
    return tBounds;
}

double XMapView::metersPerBoxel() const
{
    return metersPerBoxelAt(Mercator::yToLat(mCenterY));
}

double XMapView::metersPerBoxelAt(double pLat) const
{
    return std::cos(pLat * kPi / 180.0) * 2.0 * kPi * kEarthRadiusM / worldSize();
}

void XMapView::clampCenter()
{
    mCenterX = wrapX(mCenterX);
    mCenterY = clamp(mCenterY, 0.0, 1.0);
}
