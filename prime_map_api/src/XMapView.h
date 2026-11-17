// Haritanin "kamerasi": merkez (lat/lon), zoom, viewport ve lat/lon <-> boxel donusumu.
// Projeksiyon: Web Mercator; icerde konum Mercator biriminde (0..1, x dogu +, y kuzey +).
#ifndef PRIME_MAP_SRC_XMAPVIEW_H
#define PRIME_MAP_SRC_XMAPVIEW_H

#include "Geo.h"
#include "ScreenMath.h"

namespace Mercator
{

const double kMaxLat = 85.05112878;  // Web Mercator'un gosterebildigi en kuzey/guney enlem

double lonToX(double pLon);  // boylam -> 0..1
double latToY(double pLat);  // enlem -> 0..1 (kuzey +)
double xToLon(double pX);
double yToLat(double pY);

}  // namespace Mercator

// Gorunen alanin cografi siniri. Bati/dogu, harita 180. boylami gecerse
// -180..180 disina tasabilir (cizimde sorun cikarmaz).
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
    // 0.23: zoom sinirlari harita basina ayarlanabilir (setZoomLimits). Mutlak sinirlar:
    // kMinZoom (dunya pencereden kucuk kalmasin), kHardMaxZoom (zemin dokusu ~0,85 m/piksel;
    // daha yakini bulaniklik).
    static constexpr double kHardMaxZoom = 18.0;
    static constexpr double kTileSize = 256.0;  // zoom 0'da dunyanin kenari (boxel)

    // Haritanin cizildigi alan (global boxel). Her cizimde pencereden alinir.
    void setViewport(const RectD& pBoxels);
    const RectD& viewport() const;

    void setCenter(const LatLon& pCenter);
    LatLon center() const;
    // Merkez Mercator biriminde (0..1); XMapImageCache resmin gorunumunu bununla kurar
    double centerMercatorX() const;
    double centerMercatorY() const;
    void setCenterMercator(double pX, double pY);

    void setZoom(double pZoom);  // MinZoomLimit..MaxZoomLimit araligina kirpilir (varsayilan kMinZoom..kMaxZoom)
    double zoom() const;
    // Bu gorunumun zoom sinirlari; kMinZoom..kHardMaxZoom disina tasmaz, gecerli zoom yeni araliga kirpilir
    void setZoomLimits(double pMinZoom, double pMaxZoom);
    double minZoomLimit() const;
    double maxZoomLimit() const;

    // Zoom'daki dunyanin kenar uzunlugu (boxel): 256 * 2^zoom
    double worldSize() const;

    // lat/lon -> boxel. Boylam farki 180'i gecerse dunyanin en yakin
    // kopyasi secilir (180. boylam civarinda cizgiler kopmasin).
    void latLonToBoxel(const LatLon& pPoint, double* pX, double* pY) const;
    // Ayni donusum, cizim icin: dogrudan ekran noktasi doner
    PointF toScreen(const LatLon& pPoint) const;
    // Mercator koordinatindan (0..1) ekrana; 180. boylam sarilmasi dahil
    PointF mercatorToScreen(double pX, double pY) const;
    LatLon boxelToLatLon(double pX, double pY) const;

    void panByBoxels(double pDeltaX, double pDeltaY);  // fare suruklemesi

    // (pX, pY) altindaki yer sabit kalarak zoom'u pDelta degistirir (fare tekerlegi)
    void zoomAt(double pX, double pY, double pDelta);

    GeoBounds visibleBounds() const;

    // Herhangi bir boxel dikdortgeninin cografi siniri (kenardan tasan
    // sembolleri de yakalamak icin genisletilebilir)
    GeoBounds boundsOf(const RectD& pBoxels) const;

    // Haritanin merkezinde bir boxel kac metre (olcek cubugu icin).
    // Mercator'da olcek enleme gore degisir; merkez enlemi kullanilir.
    double metersPerBoxel() const;

    // pLat enleminde bir boxel kac metre (pist genisligi gibi yerel olculer icin)
    double metersPerBoxelAt(double pLat) const;

private:
    void clampCenter();

    RectD mViewport;
    double mCenterX = 0.5;  // Mercator birimi
    double mCenterY = 0.5;  // Mercator birimi
    double mZoom = 6.0;
    double mMinZoom = kMinZoom;
    double mMaxZoom = kMaxZoom;
};
#endif  // PRIME_MAP_SRC_XMAPVIEW_H
