// Hava yollari (earth_awy.dat, AirwayDatabase). Alcak ve yuksek hava yollari
// ayri katmandir (ayri ac/kapa dugmesi).
#pragma once

#include <cstdint>
#include <vector>

#include "Layer.h"

class AirwayDatabase;

class AirwayLayer : public Layer
{
public:
    // pHigh: true = yuksek (seviye 2), false = alcak (seviye 1) hava yollari
    AirwayLayer(const AirwayDatabase& pAirways, bool pHigh);

    void draw(const drawContext& pContext) override;
    void drawLabels(const drawContext& pContext) override;
    int labelPriority() const override
    {
        return 70;  // cok sayida: diger etiketlerden sonra, kalan bos yerlere
    }

private:
    struct VisibleSegment
    {
        int mIndex;
        float mFromX;
        float mFromY;
        float mToX;
        float mToY;
    };

    const AirwayDatabase& mAirways;
    bool mHigh;
    std::vector<int> mQuery;
    std::vector<uint32_t> mSeenStamp;  // ayni segmenti bir karede iki kez cizmemek icin
    uint32_t mStamp = 0;
    std::vector<VisibleSegment> mVisible;
};
