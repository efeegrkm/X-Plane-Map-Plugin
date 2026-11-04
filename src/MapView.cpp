// MapView.cpp
#include "MapView.h"

#include <cmath>

namespace
{

const double kPi = 3.14159265358979323846;
const double kEarthRadiusM = 6378137.0;  // Web Mercator'un kullandigi dunya yaricapi

double Clamp(double pValue, double pLow, double pHigh)
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
double WrapX(double pX)
{
    return pX - std::floor(pX);
}

}  // namespace

namespace Mercator
{

double LonToX(double pLon)
{
    return (pLon + 180.0) / 360.0;
}

double LatToY(double pLat)
{
    const double tLat = Clamp(pLat, -kMaxLat, kMaxLat) * kPi / 180.0;
    // Mercator formulu: y = ln(tan(45 + enlem/2)), sonra 0..1'e olcekle.
    // kMaxLat yuvarlanmis bir sayi oldugu icin sonuc 1'i cok az asabilir: kirp.
    return Clamp(0.5 + std::log(std::tan(kPi / 4.0 + tLat / 2.0)) / (2.0 * kPi), 0.0, 1.0);
}

double XToLon(double pX)
{
    return pX * 360.0 - 180.0;
}

double YToLat(double pY)
{
    const double tMercator = (pY - 0.5) * 2.0 * kPi;
    return (2.0 * std::atan(std::exp(tMercator)) - kPi / 2.0) * 180.0 / kPi;
}

}  // namespace Mercator

void MapView::SetViewport(const RectD& pBoxels)
{
    mViewport = pBoxels;
}

const RectD& MapView::Viewport() const
{
    return mViewport;
}

void MapView::SetCenter(const LatLon& pCenter)
{
    mCenterX = Mercator::LonToX(pCenter.mLon);
    mCenterY = Mercator::LatToY(pCenter.mLat);
    ClampCenter();
}

LatLon MapView::Center() const
{
    LatLon tCenter;
    tCenter.mLat = Mercator::YToLat(mCenterY);
    tCenter.mLon = Mercator::XToLon(mCenterX);
    return tCenter;
}

void MapView::SetZoom(double pZoom)
{
    mZoom = Clamp(pZoom, kMinZoom, kMaxZoom);
}

double MapView::Zoom() const
{
    return mZoom;
}

double MapView::WorldSize() const
{
    return kTileSize * std::pow(2.0, mZoom);
}

void MapView::LatLonToBoxel(const LatLon& pPoint, double* pX, double* pY) const
{
    const double tWorld = WorldSize();
    double tDeltaX = Mercator::LonToX(pPoint.mLon) - mCenterX;
    if (tDeltaX > 0.5)
    {
        tDeltaX -= 1.0;
    }
    else if (tDeltaX < -0.5)
    {
        tDeltaX += 1.0;
    }
    const double tDeltaY = Mercator::LatToY(pPoint.mLat) - mCenterY;

    const double tMidX = (mViewport.mLeft + mViewport.mRight) * 0.5;
    const double tMidY = (mViewport.mBottom + mViewport.mTop) * 0.5;
    *pX = tMidX + tDeltaX * tWorld;
    *pY = tMidY + tDeltaY * tWorld;
}

PointF MapView::MercatorToScreen(double pX, double pY) const
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
    const double tWorld = WorldSize();
    const double tMidX = (mViewport.mLeft + mViewport.mRight) * 0.5;
    const double tMidY = (mViewport.mBottom + mViewport.mTop) * 0.5;
    return {static_cast<float>(tMidX + tDeltaX * tWorld), static_cast<float>(tMidY + (pY - mCenterY) * tWorld)};
}

PointF MapView::ToScreen(const LatLon& pPoint) const
{
    double tX = 0.0;
    double tY = 0.0;
    LatLonToBoxel(pPoint, &tX, &tY);
    return {static_cast<float>(tX), static_cast<float>(tY)};
}

LatLon MapView::BoxelToLatLon(double pX, double pY) const
{
    const double tWorld = WorldSize();
    const double tMidX = (mViewport.mLeft + mViewport.mRight) * 0.5;
    const double tMidY = (mViewport.mBottom + mViewport.mTop) * 0.5;
    const double tMercX = WrapX(mCenterX + (pX - tMidX) / tWorld);
    const double tMercY = Clamp(mCenterY + (pY - tMidY) / tWorld, 0.0, 1.0);

    LatLon tResult;
    tResult.mLat = Mercator::YToLat(tMercY);
    tResult.mLon = Mercator::XToLon(tMercX);
    return tResult;
}

void MapView::PanByBoxels(double pDeltaX, double pDeltaY)
{
    // Icerik fareyle birlikte hareket etsin diye merkez ters yone kayar
    const double tWorld = WorldSize();
    mCenterX -= pDeltaX / tWorld;
    mCenterY -= pDeltaY / tWorld;
    ClampCenter();
}

void MapView::ZoomAt(double pX, double pY, double pDelta)
{
    const double tMidX = (mViewport.mLeft + mViewport.mRight) * 0.5;
    const double tMidY = (mViewport.mBottom + mViewport.mTop) * 0.5;

    // Imlecin altindaki nokta (Mercator birimi), zoom'dan once
    const double tOldWorld = WorldSize();
    const double tPointX = mCenterX + (pX - tMidX) / tOldWorld;
    const double tPointY = mCenterY + (pY - tMidY) / tOldWorld;

    SetZoom(mZoom + pDelta);

    // Ayni nokta yine imlecin altina dussun diye merkezi yeniden hesapla
    const double tNewWorld = WorldSize();
    mCenterX = tPointX - (pX - tMidX) / tNewWorld;
    mCenterY = tPointY - (pY - tMidY) / tNewWorld;
    ClampCenter();
}

GeoBounds MapView::VisibleBounds() const
{
    return BoundsOf(mViewport);
}

GeoBounds MapView::BoundsOf(const RectD& pBoxels) const
{
    const double tWorld = WorldSize();
    const double tMidX = (mViewport.mLeft + mViewport.mRight) * 0.5;
    const double tMidY = (mViewport.mBottom + mViewport.mTop) * 0.5;

    // Bati/dogu sarilmaz (WrapX yok): 180. boylami gecen alan -180..180 disina tasar
    GeoBounds tBounds;
    tBounds.mWest = Mercator::XToLon(mCenterX + (pBoxels.mLeft - tMidX) / tWorld);
    tBounds.mEast = Mercator::XToLon(mCenterX + (pBoxels.mRight - tMidX) / tWorld);
    tBounds.mSouth = Mercator::YToLat(Clamp(mCenterY + (pBoxels.mBottom - tMidY) / tWorld, 0.0, 1.0));
    tBounds.mNorth = Mercator::YToLat(Clamp(mCenterY + (pBoxels.mTop - tMidY) / tWorld, 0.0, 1.0));
    return tBounds;
}

double MapView::MetersPerBoxel() const
{
    return MetersPerBoxelAt(Mercator::YToLat(mCenterY));
}

double MapView::MetersPerBoxelAt(double pLat) const
{
    return std::cos(pLat * kPi / 180.0) * 2.0 * kPi * kEarthRadiusM / WorldSize();
}

void MapView::ClampCenter()
{
    mCenterX = WrapX(mCenterX);
    mCenterY = Clamp(mCenterY, 0.0, 1.0);
}
