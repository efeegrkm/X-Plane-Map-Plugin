// AircraftData.h
// Kullanicinin ucaginin konumu ve yonu, X-Plane dataref'lerinden.
// Ucak katmani, ucus izi ve "ucagi takip et" ayni veriyi buradan okur.
#pragma once

#include "XPLMDataAccess.h"

#include "Geo.h"

struct AircraftState
{
    bool mValid = false;        // ucak yuklenmis ve konum anlamli mi
    LatLon mPosition = {0.0, 0.0};
    double mHeadingDeg = 0.0;   // gercek (true) burun yonu, derece
    double mGroundSpeedKt = 0.0;
    double mAltitudeFt = 0.0;   // deniz seviyesinden
};

class AircraftData
{
public:
    void Init();     // dataref'leri bul (XPluginStart icinde)
    void Refresh();  // dataref'leri oku (ana thread'den, her karede)

    const AircraftState& State() const;

private:
    XPLMDataRef mLatRef = nullptr;
    XPLMDataRef mLonRef = nullptr;
    XPLMDataRef mHeadingRef = nullptr;
    XPLMDataRef mGroundSpeedRef = nullptr;
    XPLMDataRef mElevationRef = nullptr;
    AircraftState mState;
};
