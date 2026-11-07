// TerrainLayer.cpp
#include "layers/TerrainLayer.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "MapView.h"
#include "data/TerrainRaster.h"
#include "gfx/Canvas.h"

namespace
{

const double kMinZoom = 5.0;                   // daha uzakta zemin cizilmez (karo sayisi patlar)
const int kMaxVisibleTiles = 1500;
const size_t kTextureBudget = 512u << 20;      // video belleginde en fazla ~512 MB doku (seviye 4 parcalari 4 MB)
const int kUploadsPerFrame = 3;                // karede en fazla bu kadar doku yuklenir (FPS korumasi)
const int kFullTileStrips = 16;                // 1 derecelik dokunun Mercator seritleri
const int kChunkStrips = 4;                    // 0.25 derecelik parcanin seritleri
const int kDetailStrips = 1;                   // 1/32 derecelik parcada Mercator egriligi onemsiz
const double kDetailPixelsPerDegree = 8000.0;  // bundan yakinda zemin DSF ucgenlerinden boyanir
// Seviye 4 dokusu derece basina 32768 piksel; ekran bunun ~1,2 katini gecince
// (doku pikselleri bloklasmaya baslar) 4 kat ince seviye 5 parcalari cizilir.
const double kFinePixelsPerDegree = 40000.0;

int WrapLon(int pLon)
{
    int tLon = (pLon + 180) % 360;
    if (tLon < 0)
    {
        tLon += 360;
    }
    return tLon - 180;
}

// Enlem/boylam dikdortgenine oturan doku: pStrips yatay seride bolunur. Doku
// enlem/boylam izgarasinda, harita Mercator: her serit kendi enleminde
// cizilince Mercator'un kuzeye dogru uzamasi izlenir. Satir 0 (v = 0) = kuzey.
void AddLatLonStrips(std::vector<Gfx::TexturedVertex>& pOut, const MapView& pView, double pWest, double pSouth,
                     double pEast, double pNorth, int pStrips)
{
    for (int tStrip = 0; tStrip < pStrips; ++tStrip)
    {
        const double tTop = static_cast<double>(tStrip) / pStrips;
        const double tBottom = static_cast<double>(tStrip + 1) / pStrips;
        const double tTopLat = pNorth - (pNorth - pSouth) * tTop;
        const double tBottomLat = pNorth - (pNorth - pSouth) * tBottom;
        const PointF tTopLeft = pView.ToScreen({tTopLat, pWest});
        const PointF tTopRight = pView.ToScreen({tTopLat, pEast});
        const PointF tBottomLeft = pView.ToScreen({tBottomLat, pWest});
        const PointF tBottomRight = pView.ToScreen({tBottomLat, pEast});
        const float tV0 = static_cast<float>(tTop);
        const float tV1 = static_cast<float>(tBottom);
        pOut.push_back({tTopLeft.mX, tTopLeft.mY, 0.0f, tV0});
        pOut.push_back({tBottomLeft.mX, tBottomLeft.mY, 0.0f, tV1});
        pOut.push_back({tBottomRight.mX, tBottomRight.mY, 1.0f, tV1});
        pOut.push_back({tTopLeft.mX, tTopLeft.mY, 0.0f, tV0});
        pOut.push_back({tBottomRight.mX, tBottomRight.mY, 1.0f, tV1});
        pOut.push_back({tTopRight.mX, tTopRight.mY, 1.0f, tV0});
    }
}

}  // namespace

TerrainLayer::TerrainLayer(TerrainStore& pStore, const TerrainPalette& pPalette)
    : Layer("terrain", "Terrain (scenery)", true)
    , mStore(pStore)
    , mPalette(pPalette)
{
    mForestPart = AddSubLayer("forests", "Forests");
    mBuiltUpPart = AddSubLayer("built_up", "Built-up areas");
}

int TerrainLayer::TextureCount() const
{
    return static_cast<int>(mTextures.size());
}

int TerrainLayer::PendingTextureCount() const
{
    return mPendingLastFrame;
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
    if (pPixelsPerDegree < kDetailPixelsPerDegree)
    {
        return 3;
    }
    if (pPixelsPerDegree < kFinePixelsPerDegree)
    {
        return TerrainStore::kDetailLevel;
    }
    return TerrainStore::kFineLevel;
}

void TerrainLayer::UploadReadyImages()
{
    TerrainImage tImage;
    for (int tUpload = 0; tUpload < kUploadsPerFrame && mStore.PopImage(&tImage); ++tUpload)
    {
        const auto tExisting = mTextures.find(tImage.mKey);
        if (tExisting != mTextures.end())
        {
            if (tExisting->second.mMissingNeighbors.empty())
            {
                continue;  // ayni goruntu iki kez istenmis olabilir
            }
            // Komsusu eksik uretilmis eski doku: yenisi yerine gecer
            mTextureBytes -= tExisting->second.mBytes;
            mTextures.erase(tExisting);
        }
        CachedTexture tCached;
        tCached.mTexture = Gfx::Texture::FromRgba(tImage.mSize, tImage.mRgba.data(), true);
        tCached.mBytes = tImage.mRgba.size() * 4 / 3;  // mipmap seviyeleri dahil
        tCached.mLastFrame = mFrame;
        tCached.mMissingNeighbors = std::move(tImage.mMissingNeighbors);
        mTextureBytes += tCached.mBytes;
        mTextures.emplace(tImage.mKey, std::move(tCached));
    }
}

const Gfx::Texture* TerrainLayer::UseTexture(const TerrainImageKey& pKey)
{
    const auto tFound = mTextures.find(pKey);
    if (tFound == mTextures.end())
    {
        return nullptr;
    }
    tFound->second.mLastFrame = mFrame;
    for (const int tNeighbor : tFound->second.mMissingNeighbors)
    {
        // TileKey -> karo (SceneryIndex::TileKey'in tersi)
        if (mStore.HasElevation(tNeighbor % 360 - 180, tNeighbor / 360 - 90))
        {
            mStore.Request(pKey);  // ayni anahtar uretilirken / kuyruktayken tekrar alinmaz
            break;
        }
    }
    return &tFound->second.mTexture;
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
        mTextureBytes -= tOldest->second.mBytes;
        mTextures.erase(tOldest);  // Gfx::Texture yikicisi dokuyu siler
    }
}

void TerrainLayer::DrawDetailChunks(Gfx::Canvas& pCanvas, const MapView& pView, const GeoBounds& pBounds, int pLevel,
                                    int pLon, int pLat, double pTileWest, int pStyle, int pAreas,
                                    std::vector<Gfx::TexturedVertex>& pStrips)
{
    const int tCount = TerrainStore::ChunksPerSide(pLevel);
    const double tStep = 1.0 / tCount;
    const double tTileNorth = pLat + 1.0;
    // Ekrandaki sutun / satir araligi (satir 0 = kuzey); karonun disi kirpilir
    const int tFirstColumn = std::max(0, static_cast<int>(std::floor((pBounds.mWest - pTileWest) * tCount)));
    const int tLastColumn = std::min(tCount - 1, static_cast<int>(std::floor((pBounds.mEast - pTileWest) * tCount)));
    const int tFirstRow = std::max(0, static_cast<int>(std::floor((tTileNorth - pBounds.mNorth) * tCount)));
    const int tLastRow = std::min(tCount - 1, static_cast<int>(std::floor((tTileNorth - pBounds.mSouth) * tCount)));
    for (int tRow = tFirstRow; tRow <= tLastRow; ++tRow)
    {
        for (int tColumn = tFirstColumn; tColumn <= tLastColumn; ++tColumn)
        {
            const int tChunk = tRow * tCount + tColumn;
            const TerrainImageKey tKey = {pLon, pLat, pLevel, tChunk, pStyle, pAreas};
            const Gfx::Texture* tTexture = UseTexture(tKey);
            if (!tTexture)
            {
                mStore.Request(tKey);
                ++mPendingThisFrame;
                continue;
            }
            const double tWest = pTileWest + tColumn * tStep;
            const double tNorth = tTileNorth - tRow * tStep;
            pStrips.clear();
            AddLatLonStrips(pStrips, pView, tWest, tNorth - tStep, tWest + tStep, tNorth, kDetailStrips);
            pCanvas.DrawTextured(*tTexture, pStrips);
        }
    }
}

void TerrainLayer::Draw(const DrawContext& pContext)
{
    ++mFrame;
    mPendingLastFrame = mPendingThisFrame;
    mPendingThisFrame = 0;
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
    // Harita modu: goruntu bicimi (dokular moda gore ayri tutulur) ve deniz rengi
    const MapTheme& tTheme = *pContext.mTheme;
    const int tStyle = static_cast<int>(tTheme.mMode);
    // Asama 2: DSF vektor alanlari zemine islenir (orman sadece Scenery'nin sinif renklerinde anlamli)
    int tAreas = 0;
    if (IsSubLayerVisible(mForestPart) && tTheme.mTerrainStyle == TerrainStyle::Classes)
    {
        tAreas |= TerrainRaster::kAreaForest;
    }
    if (IsSubLayerVisible(mBuiltUpPart))
    {
        tAreas |= TerrainRaster::kAreaBuiltUp;
    }
    const TerrainColor& tWaterColor = mPalette.ColorOf(TerrainClass::Water);
    const Rgba tWater = tTheme.mTerrainStyle == TerrainStyle::Classes
                            ? Rgba{tWaterColor.mR / 255.0f, tWaterColor.mG / 255.0f, tWaterColor.mB / 255.0f, 1.0f}
                            : tTheme.mWater;

    Gfx::Canvas& tCanvas = *pContext.mCanvas;
    Gfx::Mesh tOcean;
    Gfx::Mesh tNoData;
    std::vector<Gfx::TexturedVertex> tStrips;
    for (int tLat = tSouth; tLat <= tNorth; ++tLat)
    {
        for (int tColumn = tWest; tColumn <= tEast; ++tColumn)
        {
            const int tLon = WrapLon(tColumn);
            const TileCoverage tCoverage = mStore.CoverageOf(tLon, tLat);
            const TerrainTileState tState = mStore.StateOf(tLon, tLat);
            // Ekrandaki konumu, boylam sarilmasina gore (tColumn) hesapla
            const double tTileWest = tColumn;
            const double tTileEast = tColumn + 1.0;
            const PointF tSouthWest = tView.ToScreen({static_cast<double>(tLat), tTileWest});
            const PointF tNorthEast = tView.ToScreen({tLat + 1.0, tTileEast});
            if (tCoverage == TileCoverage::NoData || tState == TerrainTileState::NoData ||
                tState == TerrainTileState::Failed)
            {
                // Kara ama sahnesi kurulu degil (ya da okunamadi): notr "veri yok" rengi, deniz degil
                tNoData.AddQuad(tSouthWest, {tNorthEast.mX, tSouthWest.mY}, tNorthEast,
                                {tSouthWest.mX, tNorthEast.mY}, tTheme.mNoData);
                continue;
            }
            if (tCoverage == TileCoverage::Ocean || tState == TerrainTileState::Ocean)
            {
                tOcean.AddQuad(tSouthWest, {tNorthEast.mX, tSouthWest.mY}, tNorthEast, {tSouthWest.mX, tNorthEast.mY},
                               tWater);
                continue;
            }

            // Tum karoyu kaplayan en iyi doku (istenen seviyeye kadar; yoksa daha kabasi)
            const int tFullLevel = std::min(tLevel, 2);
            const Gfx::Texture* tTexture = nullptr;
            for (int tTry = tFullLevel; tTry >= 0 && !tTexture; --tTry)
            {
                tTexture = UseTexture({tLon, tLat, tTry, 0, tStyle, tAreas});
            }
            if (!UseTexture({tLon, tLat, tFullLevel, 0, tStyle, tAreas}))
            {
                mStore.Request({tLon, tLat, tFullLevel, 0, tStyle, tAreas});
                ++mPendingThisFrame;
            }
            if (!UseTexture({tLon, tLat, 0, 0, tStyle, tAreas}))
            {
                mStore.Request({tLon, tLat, 0, 0, tStyle, tAreas});  // en kaba seviye hep olsun: hizli yedek
            }
            if (tTexture)
            {
                tStrips.clear();
                AddLatLonStrips(tStrips, tView, tTileWest, tLat, tTileEast, tLat + 1.0, kFullTileStrips);
                tCanvas.DrawTextured(*tTexture, tStrips);
            }

            // En ince seviye: sadece ekrana giren 0.25 derecelik parcalar
            if (tLevel >= 3)
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
                    const Gfx::Texture* tChunkTexture = UseTexture({tLon, tLat, 3, tChunk, tStyle, tAreas});
                    if (!tChunkTexture)
                    {
                        mStore.Request({tLon, tLat, 3, tChunk, tStyle, tAreas});
                        ++mPendingThisFrame;
                        continue;
                    }
                    tStrips.clear();
                    AddLatLonStrips(tStrips, tView, tChunkWest, tChunkNorth - 0.25, tChunkWest + 0.25, tChunkNorth,
                                    kChunkStrips);
                    tCanvas.DrawTextured(*tChunkTexture, tStrips);
                }
            }

            // Yakin zoom: ekrana giren 1/32 derecelik parcalar, DSF ucgenlerinden. Seviye 5'te
            // de cizilir: ince parcalar hazir olana kadar altta yedek olarak gorunur.
            if (tLevel >= TerrainStore::kDetailLevel)
            {
                DrawDetailChunks(tCanvas, tView, tBounds, TerrainStore::kDetailLevel, tLon, tLat, tTileWest, tStyle,
                                 tAreas, tStrips);
            }
            // Cok yakin zoom: 1/128 derecelik parcalar
            if (tLevel == TerrainStore::kFineLevel)
            {
                DrawDetailChunks(tCanvas, tView, tBounds, TerrainStore::kFineLevel, tLon, tLat, tTileWest, tStyle,
                                 tAreas, tStrips);
            }
        }
    }
    tCanvas.Draw(tOcean);
    tCanvas.Draw(tNoData);
    EvictUnused();
}
