// Kullanicinin ucaginin konumu ve yonu, X-Plane dataref'lerinden.
// Ucak katmani, ucus izi ve "ucagi takip et" ayni veriyi buradan okur.
#ifndef PRIME_MAP_SRC_DATA_AIRCRAFTDATA_H
#define PRIME_MAP_SRC_DATA_AIRCRAFTDATA_H

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
    void init();     // dataref'leri bul (XPluginStart icinde)
    void refresh();  // dataref'leri oku (ana thread'den, her karede)

    const AircraftState& state() const;

    // API: ucak durumunu host verir (true) ya da dataref'ler (false, varsayilan)
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
#endif  // PRIME_MAP_SRC_DATA_AIRCRAFTDATA_H
