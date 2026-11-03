// StatusBar.h
// Haritanin alt kenarindaki ince durum seridi:
//
//   41.12345 N  28.12345 E        Zoom 8.5  (mesaj)        |------| 10 NM
//   (imlecin altindaki nokta)                              (olcek cubugu)
#pragma once

#include "Geo.h"
#include "ScreenMath.h"

class MapView;

class StatusBar
{
public:
    static const int kHeight = 22;  // serit yuksekligi (boxel)

    // pHasCursor false ise (imlec harita disinda) koordinat yerine "-" yazilir.
    // pMessage: zoom'un sagina sari yazilan kisa durum yazisi (bos olabilir).
    void Draw(const RectI& pArea, const MapView& pView, bool pHasCursor, const LatLon& pCursor,
              const char* pMessage);

    bool Contains(int pX, int pY) const;

private:
    void DrawScaleBar(const MapView& pView, int pRight, int pBaseY);

    RectI mStrip;
};
