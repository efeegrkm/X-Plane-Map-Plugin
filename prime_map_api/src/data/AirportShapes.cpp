// AirportShapes.cpp
#include "data/AirportShapes.h"

#include "MapView.h"
#include "mapbox/earcut.hpp"  // third_party/earcut (dis kutuphane)

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

// earcut noktanin x'ini nth<0>, y'sini nth<1> ile okur: LocalPoint'i tanit
namespace mapbox
{
namespace util
{

template <>
struct nth<0, LocalPoint>
{
    static double get(const LocalPoint& pPoint)
    {
        return pPoint.mX;
    }
};

template <>
struct nth<1, LocalPoint>
{
    static double get(const LocalPoint& pPoint)
    {
        return pPoint.mY;
    }
};

}  // namespace util
}  // namespace mapbox

namespace
{

const double kPi = 3.14159265358979323846;
const double kMaxStepM = 2.0;          // egri uzerinde en fazla ~2 m'de bir nokta
const int kMaxSegmentPieces = 32;      // bir egri en fazla bu kadar parcaya bolunur
const double kSamePointM = 0.01;       // 1 cm'den yakin ardisik noktalar tek nokta sayilir
const double kLightSpacingM = 15.0;    // isik dizisinde lambalar arasi (orta hat isiklari 15 m civari)

double Distance(const LocalPoint& pA, const LocalPoint& pB)
{
    return std::hypot(pA.mX - pB.mX, pA.mY - pB.mY);
}

bool SamePoint(const LocalPoint& pA, const LocalPoint& pB)
{
    return Distance(pA, pB) < kSamePointM;
}

LocalPoint Mirror(const LocalPoint& pPoint, const LocalPoint& pAbout)
{
    return {2.0 * pAbout.mX - pPoint.mX, 2.0 * pAbout.mY - pPoint.mY};
}

LocalPoint Cubic(const LocalPoint& pP0, const LocalPoint& pP1, const LocalPoint& pP2, const LocalPoint& pP3,
                 double pT)
{
    const double tU = 1.0 - pT;
    const double tA = tU * tU * tU;
    const double tB = 3.0 * tU * tU * pT;
    const double tC = 3.0 * tU * pT * pT;
    const double tD = pT * pT * pT;
    return {tA * pP0.mX + tB * pP1.mX + tC * pP2.mX + tD * pP3.mX,
            tA * pP0.mY + tB * pP1.mY + tC * pP2.mY + tD * pP3.mY};
}

// Ardisik ayni noktalari ve kapali halkada ilk noktayi tekrarlayan sonu atar
std::vector<LocalPoint> Cleaned(const std::vector<LocalPoint>& pRing)
{
    std::vector<LocalPoint> tResult;
    tResult.reserve(pRing.size());
    for (const LocalPoint& tPoint : pRing)
    {
        if (tResult.empty() || !SamePoint(tResult.back(), tPoint))
        {
            tResult.push_back(tPoint);
        }
    }
    while (tResult.size() > 1 && SamePoint(tResult.front(), tResult.back()))
    {
        tResult.pop_back();
    }
    return tResult;
}

// Yerel duzlemdeki noktalari referansa gore Mercator farkina cevirir
class MercatorWriter
{
public:
    MercatorWriter(const LocalFrame& pFrame, double pOriginX, double pOriginY)
        : mFrame(pFrame)
        , mOriginX(pOriginX)
        , mOriginY(pOriginY)
    {
    }

    PointF Convert(const LocalPoint& pPoint) const
    {
        const LatLon tLatLon = mFrame.ToLatLon(pPoint);
        return {static_cast<float>(Mercator::LonToX(tLatLon.mLon) - mOriginX),
                static_cast<float>(Mercator::LatToY(tLatLon.mLat) - mOriginY)};
    }

    PointF Convert(const LatLon& pPoint) const
    {
        return {static_cast<float>(Mercator::LonToX(pPoint.mLon) - mOriginX),
                static_cast<float>(Mercator::LatToY(pPoint.mLat) - mOriginY)};
    }

    std::vector<PointF> Convert(const std::vector<LocalPoint>& pPoints) const
    {
        std::vector<PointF> tResult;
        tResult.reserve(pPoints.size());
        for (const LocalPoint& tPoint : pPoints)
        {
            const LatLon tLatLon = mFrame.ToLatLon(tPoint);
            tResult.push_back({static_cast<float>(Mercator::LonToX(tLatLon.mLon) - mOriginX),
                               static_cast<float>(Mercator::LatToY(tLatLon.mLat) - mOriginY)});
        }
        return tResult;
    }

private:
    const LocalFrame& mFrame;
    double mOriginX;
    double mOriginY;
};

// Isik tipli parcalar boyunca kLightSpacingM araliklarla lamba koyar (aralik
// parcadan parcaya tasinir: kavisli dizilerde de lambalar esit aralikli kalir)
void AddLights(const FlatChain& pChain, const MercatorWriter& pWriter, std::vector<LightPoint>& pOut)
{
    const size_t tCount = pChain.mPoints.size();
    const size_t tSegments = pChain.mClosed ? tCount : (tCount > 0 ? tCount - 1 : 0);
    double tCarry = 0.0;  // bir sonraki lambaya kalan mesafe
    for (size_t tSegment = 0; tSegment < tSegments && tSegment < pChain.mLightTypes.size(); ++tSegment)
    {
        const int tType = pChain.mLightTypes[tSegment];
        if (tType == 0)
        {
            tCarry = 0.0;
            continue;
        }
        const LocalPoint& tFrom = pChain.mPoints[tSegment];
        const LocalPoint& tTo = pChain.mPoints[(tSegment + 1) % tCount];
        const double tLength = Distance(tFrom, tTo);
        if (tCarry >= tLength)
        {
            tCarry -= tLength;  // bu parcaya lamba dusmuyor
            continue;
        }
        double tAt = tCarry;
        for (; tAt < tLength; tAt += kLightSpacingM)
        {
            const double tT = tAt / tLength;
            pOut.push_back({pWriter.Convert(LocalPoint{tFrom.mX + (tTo.mX - tFrom.mX) * tT,
                                                       tFrom.mY + (tTo.mY - tFrom.mY) * tT}),
                            tType});
        }
        tCarry = tAt - tLength;  // son lambadan sonraki lambaya kalan mesafe
    }
}

// Stili olan zincirleri (boya cizgisi / isik) cizgi listesine ekler
void AddStyledLine(const FlatChain& pChain, const MercatorWriter& pWriter, std::vector<LineMesh>& pOut)
{
    const bool tStyled = std::any_of(pChain.mLineTypes.begin(), pChain.mLineTypes.end(), [](int pType)
                                     {
                                         return pType != 0;
                                     }) ||
                         std::any_of(pChain.mLightTypes.begin(), pChain.mLightTypes.end(), [](int pType)
                                     {
                                         return pType != 0;
                                     });
    if (!tStyled || pChain.mPoints.size() < 2)
    {
        return;
    }
    LineMesh tLine;
    tLine.mPoints = pWriter.Convert(pChain.mPoints);
    tLine.mLineTypes = pChain.mLineTypes;
    tLine.mLightTypes = pChain.mLightTypes;
    tLine.mClosed = pChain.mClosed;
    pOut.push_back(std::move(tLine));
}

}  // namespace

// ============================================================================
// LocalFrame
// ============================================================================

LocalFrame::LocalFrame(const LatLon& pOrigin)
    : mOrigin(pOrigin)
    , mMetersPerDegreeLon(111320.0 * std::cos(pOrigin.mLat * kPi / 180.0))
    , mMetersPerDegreeLat(110540.0)
{
}

LocalPoint LocalFrame::ToLocal(const LatLon& pPoint) const
{
    return {(pPoint.mLon - mOrigin.mLon) * mMetersPerDegreeLon, (pPoint.mLat - mOrigin.mLat) * mMetersPerDegreeLat};
}

LatLon LocalFrame::ToLatLon(const LocalPoint& pPoint) const
{
    return {mOrigin.mLat + pPoint.mY / mMetersPerDegreeLat, mOrigin.mLon + pPoint.mX / mMetersPerDegreeLon};
}

// ============================================================================
// Shapes
// ============================================================================

namespace Shapes
{

void AddSegment(const AptNode& pFrom, const AptNode& pTo, const LocalFrame& pFrame, double pMaxStepM,
                std::vector<LocalPoint>& pOut)
{
    const LocalPoint tP0 = pFrame.ToLocal(pFrom.mPosition);
    const LocalPoint tP3 = pFrame.ToLocal(pTo.mPosition);
    pOut.push_back(tP0);
    if (!pFrom.mHasControl && !pTo.mHasControl)
    {
        return;  // duz parca
    }
    const LocalPoint tP1 = pFrom.mHasControl ? pFrame.ToLocal(pFrom.mControl) : tP0;
    const LocalPoint tP2 = pTo.mHasControl ? Mirror(pFrame.ToLocal(pTo.mControl), tP3) : tP3;
    // Kontrol cokgeninin boyu egrinin boyundan biraz uzundur: guvenli ust sinir
    const double tLength = Distance(tP0, tP1) + Distance(tP1, tP2) + Distance(tP2, tP3);
    const int tPieces = std::clamp(static_cast<int>(std::ceil(tLength / pMaxStepM)), 1, kMaxSegmentPieces);
    for (int tPiece = 1; tPiece < tPieces; ++tPiece)
    {
        pOut.push_back(Cubic(tP0, tP1, tP2, tP3, static_cast<double>(tPiece) / tPieces));
    }
}

FlatChain Flatten(const AptChain& pChain, const LocalFrame& pFrame, double pMaxStepM)
{
    FlatChain tResult;
    tResult.mClosed = pChain.mClosed;
    const size_t tCount = pChain.mNodes.size();
    if (tCount == 0)
    {
        return tResult;
    }
    const size_t tSegments = pChain.mClosed ? tCount : tCount - 1;
    for (size_t tIndex = 0; tIndex < tSegments; ++tIndex)
    {
        const AptNode& tFrom = pChain.mNodes[tIndex];
        const size_t tBefore = tResult.mPoints.size();
        AddSegment(tFrom, pChain.mNodes[(tIndex + 1) % tCount], pFrame, pMaxStepM, tResult.mPoints);
        // Bu dugumden baslayan butun alt parcalar dugumun stilini alir
        for (size_t tPiece = tBefore; tPiece < tResult.mPoints.size(); ++tPiece)
        {
            tResult.mLineTypes.push_back(tFrom.mLineTypes[0]);
            tResult.mLightTypes.push_back(tFrom.mLightType);
        }
    }
    if (!pChain.mClosed)
    {
        tResult.mPoints.push_back(pFrame.ToLocal(pChain.mNodes.back().mPosition));
    }
    return tResult;
}

double SignedArea(const std::vector<LocalPoint>& pRing)
{
    double tTwice = 0.0;
    for (size_t tIndex = 0; tIndex < pRing.size(); ++tIndex)
    {
        const LocalPoint& tA = pRing[tIndex];
        const LocalPoint& tB = pRing[(tIndex + 1) % pRing.size()];
        tTwice += tA.mX * tB.mY - tB.mX * tA.mY;
    }
    return tTwice * 0.5;
}

bool Triangulate(const std::vector<LocalPoint>& pOuter, const std::vector<std::vector<LocalPoint>>& pHoles,
                 std::vector<LocalPoint>& pOutVertices, std::vector<int>& pOutTriangles)
{
    pOutVertices.clear();
    pOutTriangles.clear();

    // earcut'in girdisi: halka listesi, [0] dis sinir, digerleri delik.
    // Cikti indeksleri halkalarin arka arkaya dizilmis noktalarina gore.
    std::vector<std::vector<LocalPoint>> tRings;
    tRings.push_back(Cleaned(pOuter));
    if (tRings[0].size() < 3)
    {
        return false;
    }
    for (const std::vector<LocalPoint>& tHole : pHoles)
    {
        std::vector<LocalPoint> tCleaned = Cleaned(tHole);
        if (tCleaned.size() >= 3)
        {
            tRings.push_back(std::move(tCleaned));
        }
    }
    for (const std::vector<LocalPoint>& tRing : tRings)
    {
        pOutVertices.insert(pOutVertices.end(), tRing.begin(), tRing.end());
    }
    for (uint32_t tIndex : mapbox::earcut<uint32_t>(tRings))
    {
        pOutTriangles.push_back(static_cast<int>(tIndex));
    }
    return !pOutTriangles.empty();
}

AirportShapes Build(const AirportDetail& pDetail, const LatLon& pReference)
{
    AirportShapes tShapes;
    const LocalFrame tFrame(pReference);
    tShapes.mOriginX = Mercator::LonToX(pReference.mLon);
    tShapes.mOriginY = Mercator::LatToY(pReference.mLat);
    const MercatorWriter tWriter(tFrame, tShapes.mOriginX, tShapes.mOriginY);

    // Yuzeyler: dosyada ilk gelen en ustte cizilir; biz alttan uste cizecegiz
    for (auto tIterator = pDetail.mPavements.rbegin(); tIterator != pDetail.mPavements.rend(); ++tIterator)
    {
        const AptPavement& tPavement = *tIterator;
        if (tPavement.mRings.empty())
        {
            continue;
        }
        std::vector<FlatChain> tRings;
        for (const AptChain& tChain : tPavement.mRings)
        {
            tRings.push_back(Flatten(tChain, tFrame, kMaxStepM));
        }
        std::vector<std::vector<LocalPoint>> tHoles;
        for (size_t tIndex = 1; tIndex < tRings.size(); ++tIndex)
        {
            tHoles.push_back(tRings[tIndex].mPoints);
        }
        std::vector<LocalPoint> tVertices;
        SurfaceMesh tMesh;
        tMesh.mSurfaceCode = tPavement.mSurfaceCode;
        tMesh.mName = tPavement.mName;
        if (Triangulate(tRings[0].mPoints, tHoles, tVertices, tMesh.mTriangles))
        {
            tMesh.mVertices = tWriter.Convert(tVertices);
            tShapes.mVertexCount += tMesh.mVertices.size();
            tShapes.mPavements.push_back(std::move(tMesh));
        }
        else
        {
            ++tShapes.mFailedPolygons;
        }
        for (const FlatChain& tRing : tRings)
        {
            tShapes.mPavementEdges.push_back(tWriter.Convert(tRing.mPoints));
            AddStyledLine(tRing, tWriter, tShapes.mLines);  // yuzey kenarindaki cizgi ve isiklar
            AddLights(tRing, tWriter, tShapes.mLights);
        }
    }

    for (const AptBoundary& tBoundary : pDetail.mBoundaries)
    {
        for (const AptChain& tChain : tBoundary.mRings)
        {
            tShapes.mBoundaries.push_back(tWriter.Convert(Flatten(tChain, tFrame, kMaxStepM).mPoints));
        }
    }

    for (const AptLinearFeature& tFeature : pDetail.mLinearFeatures)
    {
        for (const AptChain& tChain : tFeature.mChains)
        {
            const FlatChain tFlat = Flatten(tChain, tFrame, kMaxStepM);
            AddStyledLine(tFlat, tWriter, tShapes.mLines);
            AddLights(tFlat, tWriter, tShapes.mLights);
        }
    }
    // Taksi agi: dugum kimligi -> konum, sonra ucak kenarlari
    std::unordered_map<int, LatLon> tNodes;
    for (const AptTaxiNode& tNode : pDetail.mTaxiNodes)
    {
        tNodes[tNode.mId] = tNode.mPosition;
    }
    for (const AptTaxiEdge& tEdge : pDetail.mTaxiEdges)
    {
        const auto tFrom = tNodes.find(tEdge.mFrom);
        const auto tTo = tNodes.find(tEdge.mTo);
        if (tEdge.mGroundVehicles || tFrom == tNodes.end() || tTo == tNodes.end())
        {
            continue;
        }
        tShapes.mTaxiEdges.push_back(
            {tWriter.Convert(tFrom->second), tWriter.Convert(tTo->second), tEdge.mName, tEdge.mRunway, tEdge.mOneWay});
    }

    for (const LineMesh& tLine : tShapes.mLines)
    {
        tShapes.mVertexCount += tLine.mPoints.size();
    }
    tShapes.mVertexCount += tShapes.mLights.size() + 2 * tShapes.mTaxiEdges.size();
    return tShapes;
}

}  // namespace Shapes
