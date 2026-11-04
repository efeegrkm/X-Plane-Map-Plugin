// DrawingLayer.h
// Kullanicinin haritaya cizdikleri (Drawings modeli): noktalar, rotalar,
// bolgeler ve yarim kalan cizim.
//
//   nokta : beyaz halkali camgobegi isaret, etiket "P1"
//   rota  : camgobegi cizgi (buyuk daire boyunca), etiket "R1  123.4 NM"
//   bolge : yari saydam yesil dolgu + kenar, etiket "A1  45.6 NM2"
//   yarim : turuncu; son koseden imlece kesikli "lastik" cizgi
//
// Etiketler diger katmanlarinkinden oncelikli: yerleri geometri asamasinda
// (Draw) ayrilir, yazilari DrawLabels'ta cizilir.
#pragma once

#include <string>
#include <vector>

#include "Layer.h"
#include "gfx/Mesh.h"

class Drawings;
class MapView;
struct LatLon;

class DrawingLayer : public Layer
{
public:
    explicit DrawingLayer(const Drawings& pDrawings);

    void Draw(const DrawContext& pContext) override;
    void DrawLabels(const DrawContext& pContext) override;

private:
    struct PendingLabel
    {
        std::string mText;
        int mX;  // yazinin sol alt kosesi
        int mY;
    };

    // Lat/lon listesini, uzun parcalari buyuk daire boyunca bolerek ekrana cevirir
    static std::vector<Gfx::Point> ProjectPath(const MapView& pView, const std::vector<LatLon>& pPoints,
                                               bool pClosed);
    static Gfx::Point Project(const MapView& pView, const LatLon& pPoint);
    void AddLabel(const DrawContext& pContext, const std::string& pText, const Gfx::Point& pAnchor);
    // Kesikli cizgi (yarim cizimin son kosesinden imlece "lastik")
    static void AddDashedLine(const Gfx::Point& pFrom, const Gfx::Point& pTo, Gfx::OutlinedLines& pLines);

    const Drawings& mDrawings;
    std::vector<PendingLabel> mLabels;
};
