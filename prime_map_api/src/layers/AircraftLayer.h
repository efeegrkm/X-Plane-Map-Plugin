// Kullanici ucagi sembolu ve yon cizgisi.
#ifndef PRIME_MAP_SRC_LAYERS_AIRCRAFTLAYER_H
#define PRIME_MAP_SRC_LAYERS_AIRCRAFTLAYER_H

#include "Layer.h"

class AircraftData;

class AircraftLayer : public Layer
{
public:
    explicit AircraftLayer(const AircraftData& pAircraft);

    void draw(const drawContext& pContext) override;
    bool isOverlay() const override
    {
        return true;
    }

private:
    const AircraftData& mAircraft;
};
#endif
