// Haritanin alt kenarindaki ince durum seridi:
// koordinat | zoom | mesaj | olcek cubugu
#ifndef PRIME_MAP_SRC_UI_STATUSBAR_H
#define PRIME_MAP_SRC_UI_STATUSBAR_H

#include <optional>
#include <string_view>

#include "Geo.h"
#include "ScreenMath.h"

class XMapView;

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
    void draw(Gfx::Canvas& pCanvas, const RectI& pArea, const XMapView& pView, const std::optional<LatLon>& pCursor,
              std::string_view pMessage);

    bool contains(int pX, int pY) const;

private:
    static void drawScaleBar(Gfx::Canvas& pCanvas, const XMapView& pView, int pRight, int pBaseY);

    RectI mStrip;
};
#endif  // PRIME_MAP_SRC_UI_STATUSBAR_H
