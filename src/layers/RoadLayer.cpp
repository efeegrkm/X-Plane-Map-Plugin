// RoadLayer.cpp
#include "layers/RoadLayer.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "MapView.h"
#include "data/SceneryIndex.h"
#include "data/TerrainStore.h"
#include "gfx/Canvas.h"

namespace
{

// VectorLineKind sirasi: otoyol, ana, tali, yerel, demiryolu, enerji hatti
const double kMinZoom[kVectorLineKindCount] = {7.5, 9.0, 10.5, 12.0, 9.5, 11.0};
const float kWidthPixels[kVectorLineKindCount] = {2.5f, 2.0f, 1.5f, 1.0f, 1.5f, 1.0f};
// Cizim sirasi: kucuk yollar altta, otoyol ve demiryolu ustte
const int kDrawOrder[kVectorLineKindCount] = {5, 3, 2, 1, 0, 4};
const int kMaxVisibleTiles = 16;   // daha uzakta yol cizilmez (zaten esiklerin altinda)
const size_t kMaxGpuTiles = 12;    // GPU'da tutulan karo (yerel yollarla karo basina ~10-20 MB)
const uint64_t kKeepFrames = 300;  // bu kadar karedir cizilmeyen karo atilabilir

double MinVisibleZoom()
{
    return *std::min_element(std::begin(kMinZoom), std::end(kMinZoom));
}

}  // namespace

RoadLayer::RoadLayer(TerrainStore& pStore)
    : Layer("roads", "Roads and railways", true)
    , mStore(pStore)
{
    mParts[static_cast<int>(VectorLineKind::Motorway)] = AddSubLayer("motorways", "Motorways");
    mParts[static_cast<int>(VectorLineKind::Primary)] = AddSubLayer("primary", "Primary roads");
    mParts[static_cast<int>(VectorLineKind::Secondary)] = AddSubLayer("secondary", "Secondary roads");
    mParts[static_cast<int>(VectorLineKind::Local)] = AddSubLayer("local", "Local roads");
    mParts[static_cast<int>(VectorLineKind::Railway)] = AddSubLayer("railways", "Railways");
    mParts[static_cast<int>(VectorLineKind::PowerLine)] = AddSubLayer("power_lines", "Power lines", false);
}

bool RoadLayer::PrepareFrame()
{
    return TakeReadyVectors();
}

bool RoadLayer::TakeReadyVectors()
{
    bool tTaken = false;
    std::shared_ptr<const TerrainVectors> tVectors;
    while (mStore.PopVectors(&tVectors))
    {
        GpuTile& tTile = mTiles[SceneryIndex::TileKey(tVectors->West(), tVectors->South())];
        tTile.mWest = tVectors->West();
        tTile.mSouth = tVectors->South();
        tTile.mVectors = tVectors;
        tTile.mUploadedTheme = nullptr;  // (yeniden) yuklenecek
        tTile.mLastFrame = mFrame;
        tTaken = true;
    }
    return tTaken;
}

void RoadLayer::Upload(GpuTile& pTile, const MapTheme& pTheme)
{
    pTile.mOriginX = Mercator::LonToX(pTile.mWest);
    pTile.mOriginY = Mercator::LatToY(pTile.mSouth);
    std::array<std::vector<Gfx::ColorVertex>, kVectorLineKindCount> tVertices;
    for (const VectorLine& tLine : pTile.mVectors->Lines())
    {
        const int tKind = static_cast<int>(tLine.mKind);
        std::vector<Gfx::ColorVertex>& tOut = tVertices[tKind];
        const Rgba& tColor = pTheme.mRoads[tKind];
        float tPreviousX = 0.0f;
        float tPreviousY = 0.0f;
        for (size_t tIndex = 0; tIndex < tLine.mX.size(); ++tIndex)
        {
            // Mercator, karo orijinine gore (float hassasiyeti yeter: karo ~1/360)
            const float tX = static_cast<float>(Mercator::LonToX(pTile.mWest + tLine.mX[tIndex]) - pTile.mOriginX);
            const float tY = static_cast<float>(Mercator::LatToY(pTile.mSouth + tLine.mY[tIndex]) - pTile.mOriginY);
            if (tIndex > 0)
            {
                tOut.push_back({tPreviousX, tPreviousY, tColor});
                tOut.push_back({tX, tY, tColor});
            }
            tPreviousX = tX;
            tPreviousY = tY;
        }
    }
    for (int tKind = 0; tKind < kVectorLineKindCount; ++tKind)
    {
        pTile.mMeshes[tKind] = Gfx::StaticMesh();
        if (!tVertices[tKind].empty())
        {
            pTile.mMeshes[tKind].Upload(tVertices[tKind], Gfx::Primitive::Lines);
        }
    }
    pTile.mUploadedTheme = &pTheme;
}

void RoadLayer::EvictUnused()
{
    while (mTiles.size() > kMaxGpuTiles)
    {
        auto tOldest = mTiles.end();
        for (auto tIterator = mTiles.begin(); tIterator != mTiles.end(); ++tIterator)
        {
            if (tIterator->second.mLastFrame + kKeepFrames < mFrame &&
                (tOldest == mTiles.end() || tIterator->second.mLastFrame < tOldest->second.mLastFrame))
            {
                tOldest = tIterator;
            }
        }
        if (tOldest == mTiles.end())
        {
            return;  // hepsi yakinda kullanildi
        }
        mStore.ForgetVectors(tOldest->second.mWest, tOldest->second.mSouth);  // tekrar istenebilsin
        mTiles.erase(tOldest);
    }
}

void RoadLayer::Draw(const DrawContext& pContext)
{
    ++mFrame;
    TakeReadyVectors();
    const MapView& tView = *pContext.mView;
    if (tView.Zoom() < MinVisibleZoom())
    {
        return;
    }
    const GeoBounds tBounds = tView.VisibleBounds();
    const int tSouth = std::max(-90, static_cast<int>(std::floor(tBounds.mSouth)));
    const int tNorth = std::min(89, static_cast<int>(std::floor(tBounds.mNorth)));
    const int tWest = static_cast<int>(std::floor(tBounds.mWest));
    const int tEast = static_cast<int>(std::floor(tBounds.mEast));
    if ((tEast - tWest + 1) * (tNorth - tSouth + 1) > kMaxVisibleTiles)
    {
        return;
    }
    const double tScale = tView.WorldSize();  // Mercator birimi -> boxel
    std::vector<GpuTile*> tVisible;
    for (int tLat = tSouth; tLat <= tNorth; ++tLat)
    {
        for (int tColumn = tWest; tColumn <= tEast; ++tColumn)
        {
            const int tLon = ((tColumn + 180) % 360 + 360) % 360 - 180;
            const auto tFound = mTiles.find(SceneryIndex::TileKey(tLon, tLat));
            if (tFound == mTiles.end())
            {
                mStore.RequestVectors(tLon, tLat);  // deniz / veri yok karoda sonuc hic gelmez (zararsiz)
                continue;
            }
            GpuTile& tTile = tFound->second;
            tTile.mLastFrame = mFrame;
            if (tTile.mUploadedTheme != pContext.mTheme)
            {
                Upload(tTile, *pContext.mTheme);
            }
            tVisible.push_back(&tTile);
        }
    }
    Gfx::Canvas& tCanvas = *pContext.mCanvas;
    for (const int tKind : kDrawOrder)
    {
        if (!IsSubLayerVisible(mParts[tKind]) || tView.Zoom() < kMinZoom[tKind])
        {
            continue;
        }
        const float tWidth =
            std::max(1.0f, std::round(kWidthPixels[tKind] * static_cast<float>(pContext.mPixelsPerBoxel)));
        for (GpuTile* tTile : tVisible)
        {
            if (!tTile->mMeshes[tKind].IsEmpty())
            {
                tCanvas.DrawStatic(tTile->mMeshes[tKind], tView.MercatorToScreen(tTile->mOriginX, tTile->mOriginY),
                                   tScale, tWidth);
            }
        }
    }
    EvictUnused();
}
