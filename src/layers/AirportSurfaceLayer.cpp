// AirportSurfaceLayer.cpp
#include "layers/AirportSurfaceLayer.h"

#include <algorithm>
#include <cmath>

#include "MapView.h"
#include "data/AirportDetailStore.h"
#include "data/AirportGeometryStore.h"
#include "gfx/Canvas.h"

namespace
{

const double kBoundaryMinZoom = 10.5;
const double kSurfaceMinZoom = 11.0;
const double kLineMinZoom = 14.0;
const double kQueryMarginDeg = 0.05;  // havalimani referansindan disari tasan kisim icin
const size_t kMaxGpuAirports = 64;    // GPU'da tutulan en fazla havalimani

// Yuzey renkleri (koyu tema). Pistler (RunwayLayer) bunlardan biraz daha koyu.
const Rgba kAsphalt = {0.30f, 0.31f, 0.34f, 1.0f};
const Rgba kConcrete = {0.38f, 0.39f, 0.41f, 1.0f};
const Rgba kGrass = {0.30f, 0.40f, 0.24f, 1.0f};
const Rgba kDirt = {0.45f, 0.38f, 0.27f, 1.0f};
const Rgba kGravel = {0.42f, 0.42f, 0.40f, 1.0f};
const Rgba kLakebed = {0.55f, 0.50f, 0.40f, 1.0f};
const Rgba kSnow = {0.80f, 0.83f, 0.86f, 1.0f};
const Rgba kPavementEdge = {0.55f, 0.57f, 0.62f, 0.55f};
const Rgba kBoundary = {0.75f, 0.80f, 0.90f, 0.45f};
const Rgba kLoading = {0.85f, 0.90f, 1.00f, 1.0f};

// Boya cizgisi renkleri (spesifikasyon "Line Type Code")
const Rgba kLineYellow = {0.96f, 0.80f, 0.20f, 1.0f};
const Rgba kLineWhite = {0.95f, 0.95f, 0.95f, 1.0f};
const Rgba kLineRed = {0.90f, 0.22f, 0.20f, 1.0f};
const Rgba kLineOrange = {1.00f, 0.55f, 0.10f, 1.0f};
const Rgba kLineBlue = {0.30f, 0.50f, 1.00f, 1.0f};
const Rgba kLineGreen = {0.25f, 0.85f, 0.35f, 1.0f};

// Yuzey kodu -> renk. nullptr: cizilmez (su, saydam)
const Rgba* SurfaceColor(int pCode)
{
    switch (pCode)
    {
        case 2:
            return &kConcrete;
        case 3:
            return &kGrass;
        case 4:
            return &kDirt;
        case 5:
            return &kGravel;
        case 12:
            return &kLakebed;
        case 13:
        case 15:
            return nullptr;
        case 14:
            return &kSnow;
        default:
            break;
    }
    if (pCode >= 50 && pCode <= 57)
    {
        return &kConcrete;  // X-Plane 12 beton turleri
    }
    return &kAsphalt;  // 1 ve X-Plane 12 asfalt turleri (20-38)
}

// Boya cizgisi kodu -> renk. 51-64 / 70-92: ayni cizginin siyah kenarli hali.
const Rgba* LineColor(int pType)
{
    int tBase = pType;
    if (tBase >= 51 && tBase <= 64)
    {
        tBase -= 50;
    }
    else if (tBase >= 70 && tBase <= 92)
    {
        tBase -= 50;
    }
    if ((tBase >= 1 && tBase <= 14) || tBase == 19)
    {
        return &kLineYellow;
    }
    if (tBase >= 20 && tBase <= 25)
    {
        return &kLineWhite;
    }
    if (tBase >= 30 && tBase <= 32)
    {
        return &kLineRed;
    }
    switch (tBase)
    {
        case 40:
            return &kLineOrange;
        case 41:
            return &kLineBlue;
        case 42:
            return &kLineGreen;
        default:
            return nullptr;
    }
}

// Mercator farkindaki bir halkayi GL_LINES ciftlerine ekler
void AddRingLines(const std::vector<PointF>& pRing, bool pClosed, const Rgba& pColor,
                  std::vector<Gfx::ColorVertex>& pOut)
{
    if (pRing.size() < 2)
    {
        return;
    }
    const size_t tSegments = pClosed ? pRing.size() : pRing.size() - 1;
    for (size_t tIndex = 0; tIndex < tSegments; ++tIndex)
    {
        const PointF& tFrom = pRing[tIndex];
        const PointF& tTo = pRing[(tIndex + 1) % pRing.size()];
        pOut.push_back({tFrom.mX, tFrom.mY, pColor});
        pOut.push_back({tTo.mX, tTo.mY, pColor});
    }
}

}  // namespace

AirportSurfaceLayer::AirportSurfaceLayer(const AirportGeometryStore& pSummaries, AirportDetailStore& pDetails)
    : Layer("airport_surfaces", "Airport surfaces", true)
    , mSummaries(pSummaries)
    , mDetails(pDetails)
{
}

void AirportSurfaceLayer::Upload(GpuAirport& pGpu, const LoadedAirport& pLoaded) const
{
    const AirportShapes& tShapes = pLoaded.mShapes;
    pGpu.mSource = &pLoaded;
    pGpu.mOriginX = tShapes.mOriginX;
    pGpu.mOriginY = tShapes.mOriginY;

    std::vector<Gfx::ColorVertex> tVertices;
    for (const SurfaceMesh& tSurface : tShapes.mPavements)
    {
        const Rgba* tColor = SurfaceColor(tSurface.mSurfaceCode);
        if (!tColor)
        {
            continue;
        }
        for (int tIndex : tSurface.mTriangles)
        {
            const PointF& tPoint = tSurface.mVertices[tIndex];
            tVertices.push_back({tPoint.mX, tPoint.mY, *tColor});
        }
    }
    pGpu.mFills.Upload(tVertices, Gfx::Primitive::Triangles);

    tVertices.clear();
    for (const std::vector<PointF>& tRing : tShapes.mPavementEdges)
    {
        AddRingLines(tRing, true, kPavementEdge, tVertices);
    }
    pGpu.mEdges.Upload(tVertices, Gfx::Primitive::Lines);

    tVertices.clear();
    for (const std::vector<PointF>& tRing : tShapes.mBoundaries)
    {
        AddRingLines(tRing, true, kBoundary, tVertices);
    }
    pGpu.mBoundaries.Upload(tVertices, Gfx::Primitive::Lines);

    tVertices.clear();
    for (const LineMesh& tLine : tShapes.mLines)
    {
        const size_t tCount = tLine.mPoints.size();
        const size_t tSegments = tLine.mClosed ? tCount : tCount - 1;
        for (size_t tSegment = 0; tSegment < tSegments && tSegment < tLine.mLineTypes.size(); ++tSegment)
        {
            const Rgba* tColor = LineColor(tLine.mLineTypes[tSegment]);
            if (!tColor)
            {
                continue;
            }
            const PointF& tFrom = tLine.mPoints[tSegment];
            const PointF& tTo = tLine.mPoints[(tSegment + 1) % tCount];
            tVertices.push_back({tFrom.mX, tFrom.mY, *tColor});
            tVertices.push_back({tTo.mX, tTo.mY, *tColor});
        }
    }
    pGpu.mLines.Upload(tVertices, Gfx::Primitive::Lines);
}

void AirportSurfaceLayer::EvictUnused()
{
    // Ekrandan cikan havalimanlari: en uzun suredir kullanilmayani GPU'dan sil
    while (mGpu.size() > kMaxGpuAirports)
    {
        auto tOldest = mGpu.end();
        for (auto tIterator = mGpu.begin(); tIterator != mGpu.end(); ++tIterator)
        {
            if (tIterator->second.mLastFrame < mFrame &&
                (tOldest == mGpu.end() || tIterator->second.mLastFrame < tOldest->second.mLastFrame))
            {
                tOldest = tIterator;
            }
        }
        if (tOldest == mGpu.end())
        {
            return;
        }
        mGpu.erase(tOldest);
    }
}

void AirportSurfaceLayer::Draw(const DrawContext& pContext)
{
    ++mFrame;
    mDetails.BeginFrame();
    const MapView& tView = *pContext.mView;
    const double tZoom = tView.Zoom();
    if (!mSummaries.IsReady() || tZoom < kBoundaryMinZoom)
    {
        mDetails.EndFrame();
        return;
    }

    // Ekrana degen havalimanlari; merkeze yakin olan once okunsun
    const GeoBounds tScreen = tView.VisibleBounds();
    GeoBounds tQuery = tScreen;
    tQuery.mSouth -= kQueryMarginDeg;
    tQuery.mNorth += kQueryMarginDeg;
    tQuery.mWest -= kQueryMarginDeg;
    tQuery.mEast += kQueryMarginDeg;
    mQuery.clear();
    mSummaries.Query(tQuery, mQuery);
    mVisible.clear();
    for (int tIndex : mQuery)
    {
        const AirportGeometry& tAirport = mSummaries.At(tIndex);
        const GeoBounds& tBox = tAirport.mBounds;
        if (tAirport.mNodeCount == 0 || tBox.mNorth < tScreen.mSouth || tBox.mSouth > tScreen.mNorth ||
            tBox.mEast < tScreen.mWest || tBox.mWest > tScreen.mEast)
        {
            continue;
        }
        mVisible.push_back(tIndex);
    }
    const LatLon tCenter = tView.Center();
    std::sort(mVisible.begin(), mVisible.end(), [&](int pA, int pB)
              {
                  const LatLon& tA = mSummaries.At(pA).mReference;
                  const LatLon& tB = mSummaries.At(pB).mReference;
                  const double tDistA = std::hypot(tA.mLat - tCenter.mLat, tA.mLon - tCenter.mLon);
                  const double tDistB = std::hypot(tB.mLat - tCenter.mLat, tB.mLon - tCenter.mLon);
                  return tDistA < tDistB;
              });

    Gfx::Canvas& tCanvas = *pContext.mCanvas;
    const double tScale = tView.WorldSize();  // Mercator birimi -> boxel
    Gfx::Mesh tLoading;
    // Yukleniyor isareti yaklasik saniyede bir yanip soner
    const float tPulse = 0.35f + 0.35f * std::sin(pContext.mNow * 6.0f);
    std::vector<GpuAirport*> tReady;

    for (int tIndex : mVisible)
    {
        const LoadedAirport* tLoaded = mDetails.Use(tIndex);
        if (!tLoaded)
        {
            mGpu.erase(tIndex);  // ayrinti bellekten dustu: GPU kopyasi da gitsin
            const GeoBounds& tBox = mSummaries.At(tIndex).mBounds;
            const PointF tSouthWest = tView.ToScreen({tBox.mSouth, tBox.mWest});
            const PointF tNorthEast = tView.ToScreen({tBox.mNorth, tBox.mEast});
            const std::vector<PointF> tCorners = {tSouthWest,
                                                  {tNorthEast.mX, tSouthWest.mY},
                                                  tNorthEast,
                                                  {tSouthWest.mX, tNorthEast.mY}};
            for (size_t tSide = 0; tSide < tCorners.size(); ++tSide)
            {
                tLoading.AddDashedLine(tCorners[tSide], tCorners[(tSide + 1) % tCorners.size()], 1.5f, 6.0f, 5.0f,
                                       WithAlpha(kLoading, tPulse));
            }
            continue;
        }
        GpuAirport& tGpu = mGpu[tIndex];
        if (tGpu.mSource != tLoaded)
        {
            Upload(tGpu, *tLoaded);  // ilk kez (veya yeniden okundu): GPU'ya yukle
        }
        tGpu.mLastFrame = mFrame;
        tReady.push_back(&tGpu);
    }
    mDetails.EndFrame();

    // Once tum dolgular, sonra kenarlar ve cizgiler (komsu havalimanlari birbirini ortmesin)
    for (int tPass = 0; tPass < 4; ++tPass)
    {
        for (GpuAirport* tGpu : tReady)
        {
            const PointF tOffset = tView.MercatorToScreen(tGpu->mOriginX, tGpu->mOriginY);
            if (tPass == 0 && tZoom >= kSurfaceMinZoom)
            {
                tCanvas.DrawStatic(tGpu->mFills, tOffset, tScale);
            }
            else if (tPass == 1 && tZoom >= kSurfaceMinZoom)
            {
                tCanvas.DrawStatic(tGpu->mEdges, tOffset, tScale);
            }
            else if (tPass == 2)
            {
                tCanvas.DrawStatic(tGpu->mBoundaries, tOffset, tScale);
            }
            else if (tPass == 3 && tZoom >= kLineMinZoom)
            {
                tCanvas.DrawStatic(tGpu->mLines, tOffset, tScale);
            }
        }
    }
    tCanvas.Draw(tLoading);
    EvictUnused();
}
