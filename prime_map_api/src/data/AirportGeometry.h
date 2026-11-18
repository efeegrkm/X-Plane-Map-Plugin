// Havalimani ozeti: kimlik, ad, pistler, kapsadigi alan ve apt.dat icindeki yeri.
#ifndef PRIME_MAP_SRC_DATA_AIRPORTGEOMETRY_H
#define PRIME_MAP_SRC_DATA_AIRPORTGEOMETRY_H

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "Geo.h"
#include "XMapView.h"

enum class RunwaySurface : unsigned char
{
    Paved,
    Unpaved,
    Water,
    Snow,
    Transparent,
};

enum class AirportType : unsigned char
{
    Land,
    Seaplane,
    Heliport,
};

struct RunwayEnd
{
    std::string mName;
    LatLon mPosition = {0.0, 0.0};
    double mDisplacedThresholdM = 0.0;
    double mOverrunM = 0.0;
    int mMarkings = 0;
    int mApproachLights = 0;
    bool mTouchdownLights = false;
    int mReil = 0;
};

struct Runway
{
    double mWidthM = 0.0;
    RunwaySurface mSurface = RunwaySurface::Paved;
    int mSurfaceCode = 1;
    int mShoulderCode = 0;
    bool mCenterLights = false;
    int mEdgeLights = 0;
    bool mWater = false;
    RunwayEnd mEnds[2];
};

struct AptFileLocation
{
    int mFile = -1;
    uint64_t mOffset = 0;
    uint32_t mLength = 0;
};

struct AirportGeometry
{
    std::string mId;
    std::string mIcaoCode;
    std::string mName;
    AirportType mType = AirportType::Land;
    int mElevationFt = 0;
    bool mHasTower = false;
    LatLon mReference = {0.0, 0.0};
    GeoBounds mBounds;
    std::vector<Runway> mRunways;
    int mPavementCount = 0;
    int mNodeCount = 0;
    AptFileLocation mLocation;
};

struct AirportIndex
{
    std::vector<std::string> mFiles;
    std::vector<AirportGeometry> mAirports;
};

class IAirportGeometrySource
{
public:
    virtual ~IAirportGeometrySource() = default;

    virtual const char* name() const = 0;

    virtual void setCacheDirectory(const std::string& )
    {
    }

    virtual bool loadAll(const std::string& pXPlaneDirectory, const std::atomic<bool>& pCancel,
                         AirportIndex& pOut, std::string& pOutReport) = 0;
};

RunwaySurface surfaceFromAptCode(int pCode);

const char* surfaceName(int pCode);
#endif
