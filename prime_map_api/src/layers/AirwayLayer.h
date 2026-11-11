// AirwayLayer.h
// Hava yollari (earth_awy.dat, AirwayDatabase). Alcak ve yuksek hava yollari
// ayri katmandir (ayri ac/kapa dugmesi).
//
//   zoom >= 6.5  : ince cizgiler (alcak yesilimsi, yuksek mavimsi)
//   zoom >= 8    : hava yolu adlari segment ortasinda (ekranda yeterince uzunsa)
//   zoom >= 9    : tek yonlu segmentlerde yon oku
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

    void Draw(const DrawContext& pContext) override;
    void DrawLabels(const DrawContext& pContext) override;
    int LabelPriority() const override
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
