// Kullanicinin haritaya cizdikleri (Drawings modeli): noktalar, rotalar,
// bolgeler ve yarim kalan cizim. Etiketler diger katmanlarinkinden oncelikli:
// yerleri Draw'da ayrilir, yazilari drawLabels'ta cizilir.
#pragma once

#include <string>
#include <vector>

#include "Layer.h"
#include "gfx/Mesh.h"

class Drawings;
class XMapView;
struct LatLon;

class DrawingLayer : public Layer
{
public:
    explicit DrawingLayer(const Drawings& pDrawings);

    void draw(const drawContext& pContext) override;
    void drawLabels(const drawContext& pContext) override;
    // Yarim kalan cizim varken imlece giden kesikli cizgi her karede degisir: o surece ustte cizilir
    bool isOverlay() const override;

private:
    struct PendingLabel
    {
        std::string mText;
        int mX;  // yazinin sol alt kosesi
        int mY;
    };

    // Lat/lon listesini, uzun parcalari buyuk daire boyunca bolerek ekrana cevirir
    static std::vector<Gfx::Point> projectPath(const XMapView& pView, const std::vector<LatLon>& pPoints,
                                               bool pClosed);
    static Gfx::Point project(const XMapView& pView, const LatLon& pPoint);
    void addLabel(const drawContext& pContext, const std::string& pText, const Gfx::Point& pAnchor);
    // Kesikli cizgi (yarim cizimin son kosesinden imlece "lastik")
    static void addDashedLine(const Gfx::Point& pFrom, const Gfx::Point& pTo, Gfx::OutlinedLines& pLines);

    const Drawings& mDrawings;
    std::vector<PendingLabel> mLabels;
};
