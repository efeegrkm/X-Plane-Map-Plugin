// StatusBar.h
// Haritanin alt kenarindaki ince durum seridi:
//
//   41.12345 N  28.12345 E        Zoom 8.5  (mesaj)        |------| 10 NM
//   (imlecin altindaki nokta)                              (olcek cubugu)
#pragma once

#include <optional>
#include <string_view>

#include "Geo.h"
#include "ScreenMath.h"

class MapView;

namespace Gfx
{
class Canvas;
}

class StatusBar
{
public:
    static const int kHeight = 22;  // serit yuksekligi (boxel)

    // pCursor bos ise (imlec harita disinda) koordinat yerine "-" yazilir.
    // pMessage: zoom'un sagina sari yazilan kisa durum yazisi (bos olabilir).
    void Draw(Gfx::Canvas& pCanvas, const RectI& pArea, const MapView& pView, const std::optional<LatLon>& pCursor,
              std::string_view pMessage);

    bool Contains(int pX, int pY) const;

private:
    static void DrawScaleBar(Gfx::Canvas& pCanvas, const MapView& pView, int pRight, int pBaseY);

    RectI mStrip;
};
