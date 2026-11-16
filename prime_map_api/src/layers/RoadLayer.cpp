#include "layers/RoadLayer.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "XMapView.h"
#include "data/SceneryIndex.h"
#include "layers/TerrainHub.h"
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

double minVisibleZoom()
{
    return *std::min_element(std::begin(kMinZoom), std::end(kMinZoom));
}

}

RoadLayer::RoadLayer(TerrainHub& pHub)
    : Layer("roads", "Roads and railways", true)
    , mHub(pHub)
    , mUser(pHub.addUser())
{
    mParts[static_cast<int>(VectorLineKind::Motorway)] = addSubLayer("motorways", "Motorways");
    mParts[static_cast<int>(VectorLineKind::Primary)] = addSubLayer("primary", "Primary roads");
    mParts[static_cast<int>(VectorLineKind::Secondary)] = addSubLayer("secondary", "Secondary roads");
    mParts[static_cast<int>(VectorLineKind::Local)] = addSubLayer("local", "Local roads");
    mParts[static_cast<int>(VectorLineKind::Railway)] = addSubLayer("railways", "Railways");
    mParts[static_cast<int>(VectorLineKind::PowerLine)] = addSubLayer("power_lines", "Power lines", false);
}

RoadLayer::~RoadLayer()
{
    mHub.removeUser(mUser);  // bu haritanin vektor isaretleri kalkar
}

bool RoadLayer::prepareFrame()
{
    // Vektorler ortak: baska bir haritanin aldigi karolar da bu harita icin "yeni veri"dir
    mHub.takeReadyVectors();
    const bool tChanged = mHub.vectorGeneration() != mSeenGeneration;
    mSeenGeneration = mHub.vectorGeneration();
    return tChanged;
}

void RoadLayer::upload(GpuTile& pTile, const XMapTheme& pTheme)
{
    pTile.mOriginX = Mercator::lonToX(pTile.mWest);
    pTile.mOriginY = Mercator::latToY(pTile.mSouth);
    std::array<std::vector<Gfx::ColorVertex>, kVectorLineKindCount> tVertices;
    for (const VectorLine& tLine : pTile.mVectors->lines())
    {
        const int tKind = static_cast<int>(tLine.mKind);
        std::vector<Gfx::ColorVertex>& tOut = tVertices[tKind];
        const Rgba& tColor = pTheme.mRoads[tKind];
        float tPreviousX = 0.0f;
        float tPreviousY = 0.0f;
        for (size_t tIndex = 0; tIndex < tLine.mX.size(); ++tIndex)
        {
            // Mercator, karo orijinine gore (float hassasiyeti yeter: karo ~1/360)
            const float tX = static_cast<float>(Mercator::lonToX(pTile.mWest + tLine.mX[tIndex]) - pTile.mOriginX);
            const float tY = static_cast<float>(Mercator::latToY(pTile.mSouth + tLine.mY[tIndex]) - pTile.mOriginY);
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
            pTile.mMeshes[tKind].upload(tVertices[tKind], Gfx::Primitive::Lines);
        }
    }
    pTile.mUploadedTheme = &pTheme;
}

void RoadLayer::evictUnused()
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
        mHub.releaseVectors(tOldest->second.mWest, tOldest->second.mSouth, mUser);  // kimse tutmuyorsa atilir
        mTiles.erase(tOldest);
    }
}

void RoadLayer::draw(const drawContext& pContext)
{
    ++mFrame;
    mHub.takeReadyVectors();
    mSeenGeneration = mHub.vectorGeneration();
    const XMapView& tView = *pContext.mView;
    if (tView.zoom() < minVisibleZoom())
    {
        return;
    }
    const GeoBounds tBounds = tView.visibleBounds();
    const int tSouth = std::max(-90, static_cast<int>(std::floor(tBounds.mSouth)));
    const int tNorth = std::min(89, static_cast<int>(std::floor(tBounds.mNorth)));
    const int tWest = static_cast<int>(std::floor(tBounds.mWest));
    const int tEast = static_cast<int>(std::floor(tBounds.mEast));
    if ((tEast - tWest + 1) * (tNorth - tSouth + 1) > kMaxVisibleTiles)
    {
        return;
    }
    const double tScale = tView.worldSize();  // Mercator birimi -> boxel
    std::vector<GpuTile*> tVisible;
    for (int tLat = tSouth; tLat <= tNorth; ++tLat)
    {
        for (int tColumn = tWest; tColumn <= tEast; ++tColumn)
        {
            const int tLon = ((tColumn + 180) % 360 + 360) % 360 - 180;
            auto tFound = mTiles.find(SceneryIndex::tileKey(tLon, tLat));
            if (tFound == mTiles.end())
            {
                // Ortak depoda varsa al (GPU kopyasi bu haritanin); yoksa isciden istenir
                const std::shared_ptr<const TerrainVectors> tVectors = mHub.useVectors(tLon, tLat, mUser);
                if (!tVectors)
                {
                    continue;
                }
                GpuTile tNew;
                tNew.mWest = tLon;
                tNew.mSouth = tLat;
                tNew.mVectors = tVectors;
                tFound = mTiles.emplace(SceneryIndex::tileKey(tLon, tLat), std::move(tNew)).first;
            }
            GpuTile& tTile = tFound->second;
            tTile.mLastFrame = mFrame;
            if (tTile.mUploadedTheme != pContext.mTheme)
            {
                upload(tTile, *pContext.mTheme);
            }
            tVisible.push_back(&tTile);
        }
    }
    Gfx::Canvas& tCanvas = *pContext.mCanvas;
    for (const int tKind : kDrawOrder)
    {
        if (!isSubLayerDrawn(mParts[tKind]) || tView.zoom() < kMinZoom[tKind])
        {
            continue;
        }
        const float tWidth =
            std::max(1.0f, std::round(kWidthPixels[tKind] * static_cast<float>(pContext.mPixelsPerBoxel)));
        for (GpuTile* tTile : tVisible)
        {
            if (!tTile->mMeshes[tKind].isEmpty())
            {
                tCanvas.drawStatic(tTile->mMeshes[tKind], tView.mercatorToScreen(tTile->mOriginX, tTile->mOriginY),
                                   tScale, tWidth);
            }
        }
    }
    evictUnused();
}
