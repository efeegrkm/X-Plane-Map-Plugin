// Alcak ve yuksek hava yollari, adlari ve yon oklari.
#ifndef PRIME_MAP_SRC_LAYERS_AIRWAYLAYER_H
#define PRIME_MAP_SRC_LAYERS_AIRWAYLAYER_H

#include <cstdint>
#include <vector>

#include "Layer.h"

class AirwayDatabase;

class AirwayLayer : public Layer
{
public:
    AirwayLayer(const AirwayDatabase& pAirways, bool pHigh);

    void draw(const drawContext& pContext) override;
    void drawLabels(const drawContext& pContext) override;
    int labelPriority() const override
    {
        return 70;
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
    std::vector<uint32_t> mSeenStamp;
    uint32_t mStamp = 0;
    std::vector<VisibleSegment> mVisible;
};
#endif
