// Kullanici cizimlerinin harita uzerinde gosterimi.
#ifndef PRIME_MAP_SRC_LAYERS_DRAWINGLAYER_H
#define PRIME_MAP_SRC_LAYERS_DRAWINGLAYER_H

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
    bool isOverlay() const override;

private:
    struct PendingLabel
    {
        std::string mText;
        int mX;
        int mY;
    };

    static std::vector<Gfx::Point> projectPath(const XMapView& pView, const std::vector<LatLon>& pPoints,
                                               bool pClosed);
    static Gfx::Point project(const XMapView& pView, const LatLon& pPoint);
    void addLabel(const drawContext& pContext, const std::string& pText, const Gfx::Point& pAnchor);
    static void addDashedLine(const Gfx::Point& pFrom, const Gfx::Point& pTo, Gfx::OutlinedLines& pLines);

    const Drawings& mDrawings;
    std::vector<PendingLabel> mLabels;
};
#endif
