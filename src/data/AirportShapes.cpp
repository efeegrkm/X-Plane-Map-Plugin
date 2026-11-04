// AirportShapes.cpp
#include "data/AirportShapes.h"

#include "data/Earcut.h"
#include "MapView.h"

#include <algorithm>
#include <cmath>

namespace
{

const double kPi = 3.14159265358979323846;
const double kMaxStepM = 2.0;          // egri uzerinde en fazla ~2 m'de bir nokta
const int kMaxSegmentPieces = 32;      // bir egri en fazla bu kadar parcaya bolunur
const double kSamePointM = 0.01;       // 1 cm'den yakin ardisik noktalar tek nokta sayilir

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

    // Tum halkalarin noktalari tek listede; halkalar bu listeye indeks tutar
    auto tAddRing = [&](const std::vector<LocalPoint>& pRing)
    {
        std::vector<int> tIndices;
        for (const LocalPoint& tPoint : Cleaned(pRing))
        {
            tIndices.push_back(static_cast<int>(pOutVertices.size()));
            pOutVertices.push_back(tPoint);
        }
        return tIndices;
    };
    const std::vector<int> tOuter = tAddRing(pOuter);
    if (tOuter.size() < 3)
    {
        return false;
    }
    std::vector<std::vector<int>> tHoles;
    for (const std::vector<LocalPoint>& tHole : pHoles)
    {
        std::vector<int> tIndices = tAddRing(tHole);
        if (tIndices.size() >= 3)
        {
            tHoles.push_back(std::move(tIndices));
        }
    }
    pOutTriangles = Earcut::Triangulate(pOutVertices, tOuter, tHoles);
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
            AddStyledLine(Flatten(tChain, tFrame, kMaxStepM), tWriter, tShapes.mLines);
        }
    }
    for (const LineMesh& tLine : tShapes.mLines)
    {
        tShapes.mVertexCount += tLine.mPoints.size();
    }
    return tShapes;
}

}  // namespace Shapes
