// RunwayLayer.h
// Pistler: her pist, iki ucu ve genisligi ile gercek olcekli bir dikdortgen.
// Veri AirportGeometryStore'dan gelir (gecici apt.dat okuyucusu veya ekip
// motoru; katman hangisi oldugunu bilmez).
//
//   zoom >= 9    : pist dikdortgeni (yuzeye gore renk; cok dar ise en az 2 boxel)
//   zoom >= 11.5 : pist numaralari uclarin disinda
//   zoom >= 12   : blast pad / overrun (koyu)
//   zoom >= 13   : esik cizgileri ve kesikli orta hat
#pragma once

#include <vector>

#include "Layer.h"

class AirportGeometryStore;
struct Runway;

class RunwayLayer : public Layer
{
public:
    explicit RunwayLayer(const AirportGeometryStore& pStore);

    void Draw(const DrawContext& pContext) override;
    void DrawLabels(const DrawContext& pContext) override;
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
    void EmitRectangle(const ScreenRunway& pRunway, float pFrom, float pTo, float pHalfWidth) const;
    void DrawMarkings(const ScreenRunway& pRunway) const;

    const AirportGeometryStore& mStore;
    std::vector<int> mQuery;
    std::vector<ScreenRunway> mVisible;
};
