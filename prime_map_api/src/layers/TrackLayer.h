// TrackLayer.h
// Ucus izi: ucagin gectigi yerler, sari cizgi. Saniyede bir ornek alinir;
// ucak 50 m'den az hareket ettiyse kaydedilmez, 20 NM'den buyuk bir sicrama
// (konum degistirme, yeni ucus) yeni bir iz parcasi baslatir.
// En fazla kMaxPoints nokta tutulur (en eskiler silinir).
#pragma once

#include <vector>

#include "Geo.h"
#include "Layer.h"

class AircraftData;

class TrackLayer : public Layer
{
public:
    explicit TrackLayer(const AircraftData& pAircraft);

    void Update(float pNow) override;
    void Draw(const DrawContext& pContext) override;
    bool IsOverlay() const override
    {
        return true;  // ucu ucakla birlikte ilerler
    }

    void Clear();
    int PointCount() const;

private:
    void TrimOldest();

    const AircraftData& mAircraft;
    std::vector<std::vector<LatLon>> mSegments;
    float mLastSampleTime = -10.0f;
    int mPointCount = 0;
};
