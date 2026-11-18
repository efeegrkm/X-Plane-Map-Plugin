// Bir havalimaninin apt.dat ayrintisi: yuzeyler, cizgiler, isiklar, nesneler ve taksi agi.
#ifndef PRIME_MAP_SRC_DATA_AIRPORTDETAIL_H
#define PRIME_MAP_SRC_DATA_AIRPORTDETAIL_H

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "data/AirportGeometry.h"

struct AptNode
{
    LatLon mPosition;
    bool mHasControl = false;
    LatLon mControl;
    std::array<int, 2> mLineTypes = {0, 0};
    int mLightType = 0;
};

struct AptChain
{
    std::vector<AptNode> mNodes;
    bool mClosed = false;
};

struct AptPavement
{
    int mSurfaceCode = 1;
    double mSmoothness = 0.25;
    double mTextureHeadingDeg = 0.0;
    std::string mName;
    std::vector<AptChain> mRings;
};

struct AptLinearFeature
{
    std::string mName;
    std::vector<AptChain> mChains;
};

struct AptBoundary
{
    std::string mName;
    std::vector<AptChain> mRings;
};

struct AptHelipad
{
    std::string mName;
    LatLon mCenter;
    double mHeadingDeg = 0.0;
    double mLengthM = 0.0;
    double mWidthM = 0.0;
    int mSurfaceCode = 1;
    int mMarkings = 0;
    int mShoulderCode = 0;
    bool mEdgeLights = false;
};

struct AptViewpoint
{
    LatLon mPosition;
    double mHeightFt = 0.0;
    std::string mName;
};

struct AptStartLocation
{
    LatLon mPosition;
    double mHeadingDeg = 0.0;
    std::string mType;
    std::string mAircraftTypes;
    std::string mName;
    std::string mIcaoWidth;
    std::string mOperation;
    std::string mAirlines;
};

struct AptBeacon
{
    LatLon mPosition;
    int mType = 0;
    std::string mName;
};

struct AptWindsock
{
    LatLon mPosition;
    bool mLit = false;
    std::string mName;
};

struct AptSign
{
    LatLon mPosition;
    double mHeadingDeg = 0.0;
    int mSize = 1;
    std::string mText;
};

struct AptLightObject
{
    LatLon mPosition;
    int mType = 0;
    double mHeadingDeg = 0.0;
    double mGlideslopeDeg = 0.0;
};

enum class AptFrequencyKind
{
    Recorded,
    Unicom,
    Clearance,
    Ground,
    Tower,
    Approach,
    Departure,
};

struct AptFrequency
{
    AptFrequencyKind mKind = AptFrequencyKind::Recorded;
    int mKhz = 0;
    std::string mName;
};

struct AptTaxiNode
{
    LatLon mPosition;
    std::string mUsage;
    int mId = 0;
    std::string mName;
};

struct AptTaxiEdge
{
    int mFrom = 0;
    int mTo = 0;
    bool mOneWay = false;
    bool mRunway = false;
    char mWidthClass = 0;
    bool mGroundVehicles = false;
    std::string mName;
    std::vector<std::pair<std::string, std::string>> mActiveZones;
};

struct AptTruckParking
{
    LatLon mPosition;
    double mHeadingDeg = 0.0;
    std::string mType;
    int mCount = 0;
    std::string mName;
};

struct AptTruckDestination
{
    LatLon mPosition;
    double mHeadingDeg = 0.0;
    std::string mTypes;
    std::string mName;
};

struct AirportDetail
{
    std::string mId;
    std::string mName;
    AirportType mType = AirportType::Land;
    int mElevationFt = 0;
    std::vector<Runway> mRunways;

    std::vector<AptHelipad> mHelipads;
    std::vector<AptPavement> mPavements;
    std::vector<AptLinearFeature> mLinearFeatures;
    std::vector<AptBoundary> mBoundaries;
    std::vector<AptViewpoint> mViewpoints;
    std::vector<AptStartLocation> mStartLocations;
    std::vector<AptBeacon> mBeacons;
    std::vector<AptWindsock> mWindsocks;
    std::vector<AptSign> mSigns;
    std::vector<AptLightObject> mLightObjects;
    std::vector<AptFrequency> mFrequencies;
    std::vector<AptTaxiNode> mTaxiNodes;
    std::vector<AptTaxiEdge> mTaxiEdges;
    std::vector<AptTruckParking> mTruckParkings;
    std::vector<AptTruckDestination> mTruckDestinations;
    std::vector<std::pair<std::string, std::string>> mMetadata;

    int mTrafficFlowCount = 0;
    int mSkippedLineCount = 0;
    std::vector<std::string> mSkippedExamples;
};
#endif
