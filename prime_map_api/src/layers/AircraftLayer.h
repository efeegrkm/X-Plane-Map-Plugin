// Kullanicinin ucagi: burun yonune donmus sembol + yon cizgisi; ekranda
// sabit boyutta (zoom'dan bagimsiz). Katman listesinin en ustunde olmali.
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
        return true;  // her karede hareket eder: harita resminin ustune
    }

private:
    const AircraftData& mAircraft;
};
#endif  // PRIME_MAP_SRC_LAYERS_AIRCRAFTLAYER_H
