// apt.dat dugum zincirlerinden ucgenlenmis yuzeyler, cizgiler ve isik noktalari uretir.
#ifndef PRIME_MAP_SRC_DATA_AIRPORTSHAPES_H
#define PRIME_MAP_SRC_DATA_AIRPORTSHAPES_H

#include <string>
#include <vector>

#include "data/AirportDetail.h"

struct LocalPoint
{
    double mX = 0.0;
    double mY = 0.0;
};

class LocalFrame
{
public:
    explicit LocalFrame(const LatLon& pOrigin);

    LocalPoint toLocal(const LatLon& pPoint) const;
    LatLon toLatLon(const LocalPoint& pPoint) const;

private:
    LatLon mOrigin;
    double mMetersPerDegreeLon;
    double mMetersPerDegreeLat;
};

struct FlatChain
{
    std::vector<LocalPoint> mPoints;
    std::vector<int> mLineTypes;
    std::vector<int> mLightTypes;
    bool mClosed = false;
};

struct SurfaceMesh
{
    int mSurfaceCode = 1;
    std::string mName;
    std::vector<PointF> mVertices;
    std::vector<int> mTriangles;
};

struct LineMesh
{
    std::vector<PointF> mPoints;
    std::vector<int> mLineTypes;
    std::vector<int> mLightTypes;
    bool mClosed = false;
};

struct LightPoint
{
    PointF mPoint;
    int mType = 0;
};

struct TaxiEdgeShape
{
    PointF mFrom;
    PointF mTo;
    std::string mName;
    bool mRunway = false;
    bool mOneWay = false;
};

struct AirportShapes
{
    double mOriginX = 0.0;
    double mOriginY = 0.0;
    std::vector<SurfaceMesh> mPavements;
    std::vector<std::vector<PointF>> mPavementEdges;
    std::vector<std::vector<PointF>> mBoundaries;
    std::vector<LineMesh> mLines;
    std::vector<LightPoint> mLights;
    std::vector<TaxiEdgeShape> mTaxiEdges;
    int mFailedPolygons = 0;
    size_t mVertexCount = 0;
};

namespace Shapes
{
void addSegment(const AptNode& pFrom, const AptNode& pTo, const LocalFrame& pFrame, double pMaxStepM,
                std::vector<LocalPoint>& pOut);

FlatChain flatten(const AptChain& pChain, const LocalFrame& pFrame, double pMaxStepM);

double signedArea(const std::vector<LocalPoint>& pRing);

bool triangulate(const std::vector<LocalPoint>& pOuter, const std::vector<std::vector<LocalPoint>>& pHoles,
                 std::vector<LocalPoint>& pOutVertices, std::vector<int>& pOutTriangles);

AirportShapes build(const AirportDetail& pDetail, const LatLon& pReference);
}
#endif
