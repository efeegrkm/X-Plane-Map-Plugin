#include "data/AircraftData.h"

#include <cmath>

#include "Log.h"

namespace
{
const double kMetersToFeet = 3.28084;
const double kMpsToKnots = 1.943844;
}

void AircraftData::init()
{
    mLatRef = XPLMFindDataRef("sim/flightmodel/position/latitude");
    mLonRef = XPLMFindDataRef("sim/flightmodel/position/longitude");
    mHeadingRef = XPLMFindDataRef("sim/flightmodel/position/psi");
    mGroundSpeedRef = XPLMFindDataRef("sim/flightmodel/position/groundspeed");
    mElevationRef = XPLMFindDataRef("sim/flightmodel/position/elevation");
    if (!mLatRef || !mLonRef || !mHeadingRef)
    {
        log("WARNING: aircraft position datarefs not found");
    }
}

void AircraftData::refresh()
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

    mState.mValid = std::fabs(mState.mPosition.mLat) > 1e-6 || std::fabs(mState.mPosition.mLon) > 1e-6;
}

void AircraftData::setFromHost(bool pFromHost)
{
    mFromHost = pFromHost;
    refresh();
}

bool AircraftData::isFromHost() const
{
    return mFromHost;
}

void AircraftData::setHostState(const AircraftState& pState)
{
    mHostState = pState;
    if (mFromHost)
    {
        mState = pState;
    }
}

const AircraftState& AircraftData::state() const
{
    return mState;
}
