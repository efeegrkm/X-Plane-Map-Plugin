// AircraftLayer.h
// Kullanicinin ucagi: konumunda, burun yonune donmus bir ucak sembolu ve
// onunde kisa bir yon cizgisi. Ekranda sabit boyutta cizilir (zoom'dan
// bagimsiz). Katman listesinin en ustunde olmali.
#pragma once

#include "Layer.h"

class AircraftData;

class AircraftLayer : public Layer
{
public:
    explicit AircraftLayer(const AircraftData& pAircraft);

    void Draw(const DrawContext& pContext) override;

private:
    const AircraftData& mAircraft;
};
