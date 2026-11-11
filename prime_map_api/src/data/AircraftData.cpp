// AircraftData.cpp
#include "data/AircraftData.h"

#include <cmath>

#include "Log.h"

namespace
{

const double kMetersToFeet = 3.28084;
const double kMpsToKnots = 1.943844;

}  // namespace

void AircraftData::Init()
{
    mLatRef = XPLMFindDataRef("sim/flightmodel/position/latitude");           // double, derece
    mLonRef = XPLMFindDataRef("sim/flightmodel/position/longitude");          // double, derece
    mHeadingRef = XPLMFindDataRef("sim/flightmodel/position/psi");            // float, gercek yon
    mGroundSpeedRef = XPLMFindDataRef("sim/flightmodel/position/groundspeed");  // float, m/s
    mElevationRef = XPLMFindDataRef("sim/flightmodel/position/elevation");    // double, metre MSL
    if (!mLatRef || !mLonRef || !mHeadingRef)
    {
        Log("WARNING: aircraft position datarefs not found");
    }
}

void AircraftData::Refresh()
{
    if (mFromHost)
    {
        mState = mHostState;
        return;
    }
    if (!mLatRef || !mLonRef)
    {
        mState.mValid = false;
        return;
    }
    mState.mPosition.mLat = XPLMGetDatad(mLatRef);
    mState.mPosition.mLon = XPLMGetDatad(mLonRef);
    mState.mHeadingDeg = mHeadingRef ? XPLMGetDataf(mHeadingRef) : 0.0;
    mState.mGroundSpeedKt = mGroundSpeedRef ? XPLMGetDataf(mGroundSpeedRef) * kMpsToKnots : 0.0;
    mState.mAltitudeFt = mElevationRef ? XPLMGetDatad(mElevationRef) * kMetersToFeet : 0.0;

    // Ucak yuklenmeden once konum (0, 0) gelir; bunu gecerli sayma
    mState.mValid = std::fabs(mState.mPosition.mLat) > 1e-6 || std::fabs(mState.mPosition.mLon) > 1e-6;
}

void AircraftData::SetFromHost(bool pFromHost)
{
    mFromHost = pFromHost;
    Refresh();
}

bool AircraftData::IsFromHost() const
{
    return mFromHost;
}

void AircraftData::SetHostState(const AircraftState& pState)
{
    mHostState = pState;
    if (mFromHost)
    {
        mState = pState;
    }
}

const AircraftState& AircraftData::State() const
{
    return mState;
}
