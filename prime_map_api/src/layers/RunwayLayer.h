// RunwayLayer.h
// Pistler: her pist, iki ucu ve genisligi ile gercek olcekli bir dikdortgen.
// Veri AirportGeometryStore'dan gelir (gecici apt.dat okuyucusu veya dis veri
// kaynagi; katman hangisi oldugunu bilmez).
//
//   zoom >= 11   : pist dikdortgeni (yuzeye gore renk; cok dar ise en az 2 boxel)
//                  pavement (AirportSurfaceLayer kSurfaceMinZoom) ile ayni esik:
//                  kaplamalar kaybolunca pistler de kaybolur
//   zoom >= 11.5 : pist numaralari uclarin disinda
//   zoom >= 12   : blast pad / overrun (koyu)
//   zoom >= 13   : esik cizgileri ve kesikli orta hat
#pragma once

#include <string>
#include <vector>

#include "Color.h"
#include "Layer.h"

namespace Gfx
{
class Mesh;
}

class AirportGeometryStore;
struct Runway;

class RunwayLayer : public Layer
{
public:
    explicit RunwayLayer(const AirportGeometryStore& pStore);

    void Draw(const DrawContext& pContext) override;
    void DrawLabels(const DrawContext& pContext) override;
    std::string Describe(const DrawContext& pContext, double pX, double pY) const override;
    int LabelPriority() const override
    {
        return 15;
    }

private:
    // Bir pistin bu karedeki ekran geometrisi (boxel)
    struct ScreenRunway
    {
        const Runway* mRunway;
        float mEndX[2];
        float mEndY[2];
        float mDirX;       // uc 0 -> uc 1 birim yon
        float mDirY;
        float mHalfWidth;  // yari genislik
        float mLength;
        float mBoxelsPerMeter;
    };

    void CollectVisible(const DrawContext& pContext);
    // Pist ekseni boyunca pFrom..pTo (boxel, uc 0'dan), pHalfWidth genisliginde dikdortgen
    static void AddRectangle(Gfx::Mesh& pMesh, const ScreenRunway& pRunway, float pFrom, float pTo,
                             float pHalfWidth, const Rgba& pColor);
    static void AddMarkings(Gfx::Mesh& pMesh, const ScreenRunway& pRunway);

    const AirportGeometryStore& mStore;
    std::vector<int> mQuery;
    std::vector<ScreenRunway> mVisible;
};
