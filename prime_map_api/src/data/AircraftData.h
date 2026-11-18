// Kullanici ucaginin konum, yon, hiz ve irtifasi; X-Plane'den ya da host'tan.
#ifndef PRIME_MAP_SRC_DATA_AIRCRAFTDATA_H
#define PRIME_MAP_SRC_DATA_AIRCRAFTDATA_H

#include "XPLMDataAccess.h"

#include "Geo.h"

struct AircraftState
{
    bool mValid = false;
    LatLon mPosition = {0.0, 0.0};
    double mHeadingDeg = 0.0;
    double mGroundSpeedKt = 0.0;
    double mAltitudeFt = 0.0;
};

class AircraftData
{
public:
    void init();
    void refresh();

    const AircraftState& state() const;

    void setFromHost(bool pFromHost);
    bool isFromHost() const;
    void setHostState(const AircraftState& pState);

private:
    XPLMDataRef mLatRef = nullptr;
    XPLMDataRef mLonRef = nullptr;
    XPLMDataRef mHeadingRef = nullptr;
    XPLMDataRef mGroundSpeedRef = nullptr;
    XPLMDataRef mElevationRef = nullptr;
    AircraftState mState;
    AircraftState mHostState;
    bool mFromHost = false;
};
#endif
