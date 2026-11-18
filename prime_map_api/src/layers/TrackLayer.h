// Ucus izi: ucagin gectigi yolu kaydeder ve cizer.
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
        return true;
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
#endif
