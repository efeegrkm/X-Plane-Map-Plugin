// Kullanicinin ucagi: burun yonune donmus sembol + yon cizgisi; ekranda
// sabit boyutta (zoom'dan bagimsiz). Katman listesinin en ustunde olmali.
#pragma once

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
