// TerrainLayer.cpp
#include "layers/TerrainLayer.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "MapView.h"
#include "gfx/Batch.h"
#include "gfx/Texture.h"

namespace
{

const double kMinZoom = 5.0;                   // daha uzakta zemin cizilmez (karo sayisi patlar)
const int kMaxVisibleTiles = 1500;
const size_t kTextureBudget = 256u << 20;      // video belleginde en fazla ~256 MB doku
const int kUploadsPerFrame = 3;                // karede en fazla bu kadar doku yuklenir (FPS korumasi)
const int kFullTileStrips = 16;                // 1 derecelik dokunun Mercator seritleri
const int kChunkStrips = 4;                    // 0.25 derecelik parcanin seritleri

int WrapLon(int pLon)
{
    int tLon = (pLon + 180) % 360;
    if (tLon < 0)
    {
        tLon += 360;
    }
    return tLon - 180;
}

}  // namespace

TerrainLayer::TerrainLayer(TerrainStore& pStore, const TerrainPalette& pPalette)
    : Layer("terrain", "Terrain (scenery)", true)
    , mStore(pStore)
    , mPalette(pPalette)
{
}

TerrainLayer::~TerrainLayer()
{
    for (const auto& tEntry : mTextures)
    {
        Gfx::DeleteTexture(tEntry.second.mTexture);
    }
}

int TerrainLayer::TextureCount() const
{
    return static_cast<int>(mTextures.size());
}

int TerrainLayer::DesiredLevel(double pPixelsPerDegree)
{
    if (pPixelsPerDegree < 100.0)
    {
        return 0;
    }
    if (pPixelsPerDegree < 400.0)
    {
        return 1;
    }
    if (pPixelsPerDegree < 1600.0)
    {
        return 2;
    }
    return 3;
}

void TerrainLayer::UploadReadyImages()
{
    TerrainImage tImage;
    for (int tUpload = 0; tUpload < kUploadsPerFrame && mStore.PopImage(&tImage); ++tUpload)
    {
        if (mTextures.count(tImage.mKey))
        {
            continue;  // ayni goruntu iki kez istenmis olabilir
        }
        CachedTexture tCached;
        tCached.mTexture = Gfx::CreateTexture(tImage.mSize, tImage.mRgba.data());
        tCached.mBytes = tImage.mRgba.size();
        tCached.mLastFrame = mFrame;
        mTextureBytes += tCached.mBytes;
        mTextures.emplace(tImage.mKey, tCached);
    }
}

int TerrainLayer::UseTexture(const TerrainImageKey& pKey)
{
    const auto tFound = mTextures.find(pKey);
    if (tFound == mTextures.end())
    {
        return 0;
    }
    tFound->second.mLastFrame = mFrame;
    return tFound->second.mTexture;
}

void TerrainLayer::EvictUnused()
{
    // Butce asildiysa bu karede kullanilmayanlardan en eskiler silinir
    while (mTextureBytes > kTextureBudget)
    {
        auto tOldest = mTextures.end();
        for (auto tIterator = mTextures.begin(); tIterator != mTextures.end(); ++tIterator)
        {
            if (tIterator->second.mLastFrame < mFrame &&
                (tOldest == mTextures.end() || tIterator->second.mLastFrame < tOldest->second.mLastFrame))
            {
                tOldest = tIterator;
            }
        }
        if (tOldest == mTextures.end())
        {
            return;  // hepsi su an ekranda
        }
        Gfx::DeleteTexture(tOldest->second.mTexture);
        mTextureBytes -= tOldest->second.mBytes;
        mTextures.erase(tOldest);
    }
}

void TerrainLayer::Draw(const DrawContext& pContext)
{
    ++mFrame;
    mStore.BeginFrame();
    UploadReadyImages();

    const MapView& tView = *pContext.mView;
    if (!mStore.IsIndexReady() || tView.Zoom() < kMinZoom)
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
    const double tPixelsPerDegree = tView.WorldSize() / 360.0 * pContext.mPixelsPerBoxel;
    const int tLevel = DesiredLevel(tPixelsPerDegree);
    const TerrainColor& tWaterColor = mPalette.ColorOf(TerrainClass::Water);
    const Rgba tWater = {tWaterColor.mR / 255.0f, tWaterColor.mG / 255.0f, tWaterColor.mB / 255.0f, 1.0f};

    Gfx::TriangleBatch tOcean;
    for (int tLat = tSouth; tLat <= tNorth; ++tLat)
    {
        for (int tColumn = tWest; tColumn <= tEast; ++tColumn)
        {
            const int tLon = WrapLon(tColumn);
            const TileCoverage tCoverage = mStore.CoverageOf(tLon, tLat);
            const TerrainTileState tState = mStore.StateOf(tLon, tLat);
            if (tCoverage == TileCoverage::NoData || tState == TerrainTileState::NoData ||
                tState == TerrainTileState::Failed)
            {
                continue;  // bu bolge kurulu degil: harita zemini (koyu) gorunur
            }
            // Ekrandaki konumu, boylam sarilmasina gore (tColumn) hesapla
            const double tTileWest = tColumn;
            const double tTileEast = tColumn + 1.0;
            if (tCoverage == TileCoverage::Ocean || tState == TerrainTileState::Ocean)
            {
                double tX0 = 0.0;
                double tY0 = 0.0;
                double tX1 = 0.0;
                double tY1 = 0.0;
                tView.LatLonToBoxel({static_cast<double>(tLat), tTileWest}, &tX0, &tY0);
                tView.LatLonToBoxel({tLat + 1.0, tTileEast}, &tX1, &tY1);
                tOcean.AddQuad({static_cast<float>(tX0), static_cast<float>(tY0)},
                               {static_cast<float>(tX1), static_cast<float>(tY0)},
                               {static_cast<float>(tX1), static_cast<float>(tY1)},
                               {static_cast<float>(tX0), static_cast<float>(tY1)}, tWater);
                continue;
            }

            // Tum karoyu kaplayan en iyi doku (istenen seviyeye kadar; yoksa daha kabasi)
            const int tFullLevel = std::min(tLevel, 2);
            int tTexture = 0;
            for (int tTry = tFullLevel; tTry >= 0 && tTexture == 0; --tTry)
            {
                tTexture = UseTexture({tLon, tLat, tTry, 0});
            }
            if (UseTexture({tLon, tLat, tFullLevel, 0}) == 0)
            {
                mStore.Request({tLon, tLat, tFullLevel, 0});
            }
            if (UseTexture({tLon, tLat, 0, 0}) == 0)
            {
                mStore.Request({tLon, tLat, 0, 0});  // en kaba seviye hep olsun: hizli yedek
            }
            if (tTexture != 0)
            {
                Gfx::DrawTextureLatLon(tTexture, tView, tTileWest, tLat, tTileEast, tLat + 1.0, kFullTileStrips);
            }

            // En ince seviye: sadece ekrana giren 0.25 derecelik parcalar
            if (tLevel == 3)
            {
                for (int tChunk = 0; tChunk < 16; ++tChunk)
                {
                    const double tChunkWest = tTileWest + (tChunk % 4) * 0.25;
                    const double tChunkNorth = tLat + 1.0 - (tChunk / 4) * 0.25;
                    if (tChunkWest + 0.25 < tBounds.mWest || tChunkWest > tBounds.mEast ||
                        tChunkNorth < tBounds.mSouth || tChunkNorth - 0.25 > tBounds.mNorth)
                    {
                        continue;
                    }
                    const int tChunkTexture = UseTexture({tLon, tLat, 3, tChunk});
                    if (tChunkTexture == 0)
                    {
                        mStore.Request({tLon, tLat, 3, tChunk});
                        continue;
                    }
                    Gfx::DrawTextureLatLon(tChunkTexture, tView, tChunkWest, tChunkNorth - 0.25, tChunkWest + 0.25,
                                           tChunkNorth, kChunkStrips);
                }
            }
        }
    }
    tOcean.Draw();
    EvictUnused();
}
