// API'nin ortak tipleri, numaralandirmalari ve surum bilgisi.
#ifndef PRIME_MAP_SRC_API_XMAPTYPES_H
#define PRIME_MAP_SRC_API_XMAPTYPES_H

#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "primemapglobal.h"

namespace prime
{
constexpr int kVersionMajor = 1;
constexpr int kVersionMinor = 0;
constexpr int kVersionPatch = 0;
PRIME_MAP_EXPORT std::string versionString();
PRIME_MAP_EXPORT std::string libraryPath();

inline std::string hostVersionString()
{
    return std::format("{}.{}.{}", kVersionMajor, kVersionMinor, kVersionPatch);
}

inline bool isLibraryVersionMatching()
{
    return versionString() == hostVersionString();
}

struct LatLon
{
    double mLat = 0.0;
    double mLon = 0.0;
};

struct GeoBounds
{
    double mSouth = 0.0;
    double mWest = 0.0;
    double mNorth = 0.0;
    double mEast = 0.0;
};

struct Geometry
{
    int mLeft = 0;
    int mTop = 0;
    int mWidth = 0;
    int mHeight = 0;
};

enum class XMapMode
{
    Scenery,
    Vfr,
    IfrLow,
    IfrHigh,
    Dark,
};

enum class WindowStyle
{
    Embedded,
    Standalone,
    Texture,
};

enum class MouseAction
{
    Down,
    Drag,
    Up,
};

enum class MouseButton
{
    Left,
    Right,
};

enum class DrawTool
{
    Pan,
    Point,
    Route,
    Region,
};

enum class DataSource
{
    XPlane,
    Host,
};

enum class Persistence
{
    Off,
    SaveOnly,
    LoadAndSave,
};

struct Status
{
    bool mNavReady = false;
    bool mAirportsReady = false;
    bool mAirwaysReady = false;
    bool mTerrainIndexReady = false;
    int mTerrainPendingImages = 0;
    bool mPrecacheRunning = false;
    int mPrecacheDone = 0;
    int mPrecacheTotal = 0;
    std::string mText;
    std::string mPerformanceText;
    int mOpenMaps = 0;
    int mTerrainCacheLimitMb = 0;
    int mTextureLimitMb = 0;
};

enum class XMapLayer
{
    XMapGroup, Terrain, TerrainForests, TerrainBuiltUp,
    Roads, RoadsMotorways, RoadsPrimary, RoadsSecondary, RoadsLocal, RoadsRailways, RoadsPowerLines,
    Grid,
    AirportsGroup, AirportSymbols,
    Runways, RunwaysPaved, RunwaysUnpaved, RunwaysWater, RunwaysOverruns, RunwaysMarkings, RunwaysNumbers,
    Surfaces, SurfacesPavement, SurfacesPavementEdges, SurfacesBoundary, SurfacesMarkings,
    Objects, ObjectsLighting, ObjectsHelipads, ObjectsBeacons, ObjectsWindsocks, ObjectsGates, ObjectsTieDowns,
    ObjectsHangars, ObjectsOtherStarts, ObjectsStartNames, ObjectsApproachLights, ObjectsSigns,
    TaxiRoutes,
    NavaidsGroup, Vor, Ndb, Ils, Fixes,
    AirwaysGroup, AirwaysLow, AirwaysHigh,
    FlightGroup, Aircraft, FmsRoute, FlightTrack,
    Drawings, DrawingsPoints, DrawingsRoutes, DrawingsRegions, DrawingsLabels,
    Count
};

enum class LayerKind
{
    Group,
    Layer,
    SubLayer,
};

enum class LayerState
{
    Off,
    On,
    Mixed,
};

struct AirportMatch
{
    std::string mIdent;
    std::string mName;
    LatLon mPosition;
};

struct XMapObject
{
    XMapLayer mLayer = XMapLayer::Count;
    std::string mText;
};

enum class ShapeKind
{
    Point,
    Route,
    Region,
};

struct Shape
{
    int mId = 0;
    ShapeKind mKind = ShapeKind::Point;
    std::string mLabel;
    std::vector<LatLon> mPoints;
};

struct AircraftState
{
    bool mValid = false;
    LatLon mPosition;
    double mHeadingDeg = 0.0;
    double mGroundSpeedKt = 0.0;
    double mAltitudeFt = 0.0;
};

struct RouteWaypoint
{
    std::string mIdent;
    LatLon mPosition;
    int mAltitudeFt = 0;
};

struct Rgb
{
    int mR = 0;
    int mG = 0;
    int mB = 0;
};

struct TerrainSettings
{
    bool mUseXPlaneColors = true;
    std::vector<std::pair<std::string, Rgb>> mClassColors;
    double mShadingStrength = 0.0;
    double mBlendMeters = 0.0;
    double mBlendMaxPixels = 0.0;
    double mWarpMeters = 0.0;
    double mWarpWavelength = 0.0;
    double mCloseupClassMix = 0.0;
    bool mReliefMultiDirectional = true;
    double mReliefExaggeration = 0.0;
    double mReliefCoarseBoost = 0.0;
    double mReliefSlopeTone = 0.0;
    double mReliefFlatSlope = 0.0;
    bool mCoastline = true;
    double mCoastlineStrength = 0.0;
    double mWaterBandMeters = 0.0;
    double mWaterBandMaxPixels = 0.0;
    double mWaterBandStrength = 0.0;
    double mDepthTone = 0.0;
    double mDepthFullMeters = 0.0;
    bool mVfrBands = true;
    bool mVfrContourLines = false;
    double mVfrContourStrength = 0.0;
    std::vector<std::pair<double, Rgb>> mVfrBandsFeet;
};

struct XMapConfig
{
    std::string mName;
    Persistence mPersistence = Persistence::LoadAndSave;
    Geometry mGeometry;
    WindowStyle mStyle = WindowStyle::Texture;
    XMapMode mMode = XMapMode::Dark;
    bool mUiVisible = false;
    bool mMouseInput = false;
    bool mVisible = true;
    bool mImageCache = true;
    int mMaxRenderFps = 15;
    bool mXPlaneFont = false;
};

struct XMapManagerSettings
{
    int mTerrainCacheMb = 1024;
    int mCacheGrowthPercentPerXMap = 25;
    int mTerrainCacheMaxMb = 0;
    int mPrefetchTiles = 20;
    int mTextureMemoryMb = 512;
    int mMemoryGrowthPercentPerXMap = 50;
    int mTextureMemoryMaxMb = 2048;
    bool mAutoSave = true;
    double mAutoSaveSeconds = 2.0;
};
}

#endif
