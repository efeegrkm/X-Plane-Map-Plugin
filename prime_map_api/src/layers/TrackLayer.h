// Ucus izi: saniyede bir ornek alinir; 50 m'den az hareket kaydedilmez,
// 20 NM'den buyuk sicrama (yeni ucus) yeni iz parcasi baslatir.
// En fazla kMaxPoints nokta tutulur (en eskiler silinir).
#ifndef PRIME_MAP_SRC_LAYERS_TRACKLAYER_H
#define PRIME_MAP_SRC_LAYERS_TRACKLAYER_H

#include <vector>

#include "Geo.h"
#include "Layer.h"

class AircraftData;

class TrackLayer : public Layer
{
public:
    explicit TrackLayer(const AircraftData& pAircraft);

    void update(float pNow) override;
    void draw(const drawContext& pContext) override;
    bool isOverlay() const override
    {
        return true;  // ucu ucakla birlikte ilerler
    }

    void clear();
    int pointCount() const;

private:
    void trimOldest();

    const AircraftData& mAircraft;
    std::vector<std::vector<LatLon>> mSegments;
    float mLastSampleTime = -10.0f;
    int mPointCount = 0;
};
#endif  // PRIME_MAP_SRC_LAYERS_TRACKLAYER_H
