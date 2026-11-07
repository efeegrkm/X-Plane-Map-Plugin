// MapView.h
// Haritanin "kamerasi": hangi noktaya, hangi zoom ile bakiyoruz ve
// lat/lon <-> ekran (boxel) donusumu. X-Plane'e bagimli DEGIL.
//
// Projeksiyon: Web Mercator (internet haritalarinin ve zemin karolarinin
// kullandigi projeksiyon). Dunya, zoom 0'da 256x256 boxel'lik bir kare;
// her zoom adiminda kenar 2 katina cikar. Zoom surekli bir sayidir
// (7.5 gibi ara degerler olabilir).
//
// Icerde konum "Mercator birimi" ile tutulur: dunyanin tamami x ve y
// yonunde 0..1 araligi. x doguya, y kuzeye dogru artar (boxel ile ayni yon).
#pragma once

#include "Geo.h"
#include "ScreenMath.h"

namespace Mercator
{

const double kMaxLat = 85.05112878;  // Web Mercator'un gosterebildigi en kuzey/guney enlem

double LonToX(double pLon);  // boylam -> 0..1
double LatToY(double pLat);  // enlem -> 0..1 (kuzey +)
double XToLon(double pX);
double YToLat(double pY);

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

class MapView
{
public:
    static constexpr double kMinZoom = 3.0;
    static constexpr double kMaxZoom = 16.5;  // son 3 tekerlek kademesi kapali (18 - 3 x 0.5): gereksiz yakin bakis
    static constexpr double kTileSize = 256.0;  // zoom 0'da dunyanin kenari (boxel)

    // Haritanin cizildigi alan (global boxel). Her cizimde pencereden alinir.
    void SetViewport(const RectD& pBoxels);
    const RectD& Viewport() const;

    void SetCenter(const LatLon& pCenter);
    LatLon Center() const;
    // Merkez Mercator biriminde (0..1); MapImageCache resmin gorunumunu bununla kurar
    double CenterMercatorX() const;
    double CenterMercatorY() const;
    void SetCenterMercator(double pX, double pY);

    void SetZoom(double pZoom);  // kMinZoom..kMaxZoom araligina kirpilir
    double Zoom() const;

    // Zoom'daki dunyanin kenar uzunlugu (boxel): 256 * 2^zoom
    double WorldSize() const;

    // lat/lon -> boxel. Boylam farki 180'i gecerse dunyanin en yakin
    // kopyasi secilir (180. boylam civarinda cizgiler kopmasin).
    void LatLonToBoxel(const LatLon& pPoint, double* pX, double* pY) const;
    // Ayni donusum, cizim icin: dogrudan ekran noktasi doner
    PointF ToScreen(const LatLon& pPoint) const;
    // Mercator koordinatindan (0..1) ekrana; 180. boylam sarilmasi dahil
    PointF MercatorToScreen(double pX, double pY) const;
    LatLon BoxelToLatLon(double pX, double pY) const;

    // Goruntuyu pDeltaX/pDeltaY boxel kaydirir (fare ile surukleme:
    // fare saga giderse harita da saga gider).
    void PanByBoxels(double pDeltaX, double pDeltaY);

    // (pX, pY) noktasinin altindaki yer sabit kalacak sekilde zoom'u
    // pDelta kadar degistirir (fare tekerlegi).
    void ZoomAt(double pX, double pY, double pDelta);

    GeoBounds VisibleBounds() const;

    // Herhangi bir boxel dikdortgeninin cografi siniri (ornek: kenarlardan
    // biraz tasan sembolleri de bulmak icin genisletilmis alan)
    GeoBounds BoundsOf(const RectD& pBoxels) const;

    // Haritanin merkezinde bir boxel kac metre (olcek cubugu icin).
    // Mercator'da olcek enleme gore degisir; merkez enlemi kullanilir.
    double MetersPerBoxel() const;

    // pLat enleminde bir boxel kac metre (pist genisligi gibi yerel olculer icin)
    double MetersPerBoxelAt(double pLat) const;

private:
    void ClampCenter();

    RectD mViewport;
    double mCenterX = 0.5;  // Mercator birimi
    double mCenterY = 0.5;  // Mercator birimi
    double mZoom = 6.0;
};
