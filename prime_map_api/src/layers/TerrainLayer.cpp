#include "layers/TerrainLayer.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "XMapView.h"
#include "data/TerrainRaster.h"
#include "gfx/Canvas.h"

namespace
{

const int kMaxVisibleTiles = 1500;
const int kFullTileStrips = 16;
const int kChunkStrips = 4;
const int kDetailStrips = 1;
const double kDetailPixelsPerDegree = 8000.0;  // bundan yakinda zemin DSF ucgenlerinden boyanir
// Seviye 4 dokusu derece basina 32768 piksel; ekran bunu gecince (pikseller
// bloklasmaya baslar) 4 kat ince seviye 5 parcalari cizilir.
const double kFinePixelsPerDegree = 40000.0;

int wrapLon(int pLon)
{
    int tLon = (pLon + 180) % 360;
    if (tLon < 0)
    {
        tLon += 360;
    }
    return tLon - 180;
}

// Enlem/boylam dikdortgenine oturan doku: pStrips yatay seride bolunur. Doku
// lat/lon izgarasinda, harita Mercator: her serit kendi enleminde cizilince
// Mercator'un kuzeye dogru uzamasi izlenir. Satir 0 (v = 0) = kuzey.
void addLatLonStrips(std::vector<Gfx::TexturedVertex>& pOut, const XMapView& pView, double pWest, double pSouth,
                     double pEast, double pNorth, int pStrips)
{
    for (int tStrip = 0; tStrip < pStrips; ++tStrip)
    {
        const double tTop = static_cast<double>(tStrip) / pStrips;
        const double tBottom = static_cast<double>(tStrip + 1) / pStrips;
        const double tTopLat = pNorth - (pNorth - pSouth) * tTop;
        const double tBottomLat = pNorth - (pNorth - pSouth) * tBottom;
        const PointF tTopLeft = pView.toScreen({tTopLat, pWest});
        const PointF tTopRight = pView.toScreen({tTopLat, pEast});
        const PointF tBottomLeft = pView.toScreen({tBottomLat, pWest});
        const PointF tBottomRight = pView.toScreen({tBottomLat, pEast});
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

}

TerrainLayer::TerrainLayer(TerrainHub& pHub, const TerrainPalette& pPalette, int pPaletteId)
    : Layer("terrain", "Terrain (scenery)", true)
    , mHub(pHub)
    , mStore(pHub.store())
    , mPalette(pPalette)
    , mPaletteId(pPaletteId)
    , mUser(pHub.addUser())
{
    mForestPart = addSubLayer("forests", "Forests");
    mBuiltUpPart = addSubLayer("built_up", "Built-up areas");
}

TerrainLayer::~TerrainLayer()
{
    mHub.removeUser(mUser);  // bu haritanin doku isaretleri kalkar
}

void TerrainLayer::setPalette(const TerrainPalette& pPalette, int pPaletteId)
{
    mPalette = pPalette;
    mPaletteId = pPaletteId;
}

int TerrainLayer::paletteId() const
{
    return mPaletteId;
}

int TerrainLayer::textureCount() const
{
    return mHub.textureCount();
}

TerrainImageKey TerrainLayer::keyOf(int pLon, int pLat, int pLevel, int pChunk, int pStyle, int pAreas) const
{
    TerrainImageKey tKey;
    tKey.mWest = pLon;
    tKey.mSouth = pLat;
    tKey.mLevel = pLevel;
    tKey.mChunk = pChunk;
    tKey.mStyle = pStyle;
    tKey.mAreas = pAreas;
    tKey.mPalette = mPaletteId;
    return tKey;
}

int TerrainLayer::pendingTextureCount() const
{
    return mPendingLastFrame;
}

int TerrainLayer::desiredLevel(double pPixelsPerDegree)
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

bool TerrainLayer::prepareFrame()
{
    // Dokular ortak: baska bir haritanin cizimi sirasinda yuklenmis olanlar da "yeni veri"dir
    mHub.uploadReady();
    const bool tChanged = mHub.textureGeneration() != mSeenGeneration;
    mSeenGeneration = mHub.textureGeneration();
    return tChanged;
}

const Gfx::Texture* TerrainLayer::useTexture(const TerrainImageKey& pKey)
{
    return mHub.useTexture(pKey, mUser);
}

void TerrainLayer::drawDetailChunks(Gfx::Canvas& pCanvas, const XMapView& pView, const GeoBounds& pBounds, int pLevel,
                                    int pLon, int pLat, double pTileWest, int pStyle, int pAreas,
                                    std::vector<Gfx::TexturedVertex>& pStrips)
{
    const int tCount = TerrainStore::chunksPerSide(pLevel);
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
            const TerrainImageKey tKey = keyOf(pLon, pLat, pLevel, tChunk, pStyle, pAreas);
            const Gfx::Texture* tTexture = useTexture(tKey);
            if (!tTexture)
            {
                mStore.Request(tKey);
                ++mPendingThisFrame;
                continue;
            }
            const double tWest = pTileWest + tColumn * tStep;
            const double tNorth = tTileNorth - tRow * tStep;
            pStrips.clear();
            addLatLonStrips(pStrips, pView, tWest, tNorth - tStep, tWest + tStep, tNorth, kDetailStrips);
            pCanvas.drawTextured(*tTexture, pStrips);
        }
    }
}

void TerrainLayer::draw(const drawContext& pContext)
{
    mPendingLastFrame = mPendingThisFrame;
    mPendingThisFrame = 0;
    mStore.beginFrame();
    mHub.uploadReady();
    mSeenGeneration = mHub.textureGeneration();
    mHub.beginDraw(mUser);  // bu haritanin doku isaretleri bu cizimde yeniden konur

    const XMapView& tView = *pContext.mView;
    if (!mStore.isIndexReady() || !isInZoomRange())  // yerlesik esik 5.0 (LayerTable tablosu)
    {
        return;
    }
    mStore.setFocus(tView.center().mLat, tView.center().mLon);  // merkezden kenara oncelik
    const GeoBounds tBounds = tView.visibleBounds();
    const int tSouth = std::max(-90, static_cast<int>(std::floor(tBounds.mSouth)));
    const int tNorth = std::min(89, static_cast<int>(std::floor(tBounds.mNorth)));
    const int tWest = static_cast<int>(std::floor(tBounds.mWest));
    const int tEast = static_cast<int>(std::floor(tBounds.mEast));
    if ((tEast - tWest + 1) * (tNorth - tSouth + 1) > kMaxVisibleTiles)
    {
        return;
    }
    const double tPixelsPerDegree = tView.worldSize() / 360.0 * pContext.mPixelsPerBoxel;
    const int tLevel = desiredLevel(tPixelsPerDegree);
    const XMapTheme& tTheme = *pContext.mTheme;
    const int tStyle = static_cast<int>(tTheme.mMode);
    // Asama 2: DSF vektor alanlari zemine islenir (orman sadece sinif renklerinde anlamli)
    int tAreas = 0;
    if (isSubLayerDrawn(mForestPart) && tTheme.mTerrainStyle == TerrainStyle::Classes)
    {
        tAreas |= TerrainRaster::kAreaForest;
    }
    if (isSubLayerDrawn(mBuiltUpPart))
    {
        tAreas |= TerrainRaster::kAreaBuiltUp;
    }
    const TerrainColor& tWaterColor = mPalette.colorOf(TerrainClass::Water);
    const Rgba tWater = tTheme.mTerrainStyle == TerrainStyle::Classes
                            ? Rgba{tWaterColor.mR / 255.0f, tWaterColor.mG / 255.0f, tWaterColor.mB / 255.0f, 1.0f}
                            : tTheme.mWater;

    Gfx::Canvas& tCanvas = *pContext.mCanvas;
    Gfx::Mesh tOcean;
    Gfx::Mesh tNoData;
    // Yer tutucu: sahnesi kurulu ama hic dokusu gelmemis karo siyah kalmasin
    const bool tPlaceholder = tTheme.mTerrainStyle == TerrainStyle::Classes;
    const TerrainColor tLandColor = mPalette.referenceLandColor();
    const Rgba tLand = {tLandColor.mR / 255.0f, tLandColor.mG / 255.0f, tLandColor.mB / 255.0f, 1.0f};
    Gfx::Mesh tLoading;
    bool tHasLoading = false;
    std::vector<Gfx::TexturedVertex> tStrips;
    for (int tLat = tSouth; tLat <= tNorth; ++tLat)
    {
        for (int tColumn = tWest; tColumn <= tEast; ++tColumn)
        {
            const int tLon = wrapLon(tColumn);
            const TileCoverage tCoverage = mStore.coverageOf(tLon, tLat);
            const TerrainTileState tState = mStore.stateOf(tLon, tLat);
            // Ekrandaki konumu, boylam sarilmasina gore (tColumn) hesapla
            const double tTileWest = tColumn;
            const double tTileEast = tColumn + 1.0;
            const PointF tSouthWest = tView.toScreen({static_cast<double>(tLat), tTileWest});
            const PointF tNorthEast = tView.toScreen({tLat + 1.0, tTileEast});
            if (tCoverage == TileCoverage::NoData || tState == TerrainTileState::NoData ||
                tState == TerrainTileState::Failed)
            {
                // Kara ama sahnesi kurulu degil (ya da okunamadi): notr "veri yok" rengi, deniz degil
                tNoData.addQuad(tSouthWest, {tNorthEast.mX, tSouthWest.mY}, tNorthEast,
                                {tSouthWest.mX, tNorthEast.mY}, tTheme.mNoData);
                continue;
            }
            if (tCoverage == TileCoverage::Ocean || tState == TerrainTileState::Ocean)
            {
                tOcean.addQuad(tSouthWest, {tNorthEast.mX, tSouthWest.mY}, tNorthEast, {tSouthWest.mX, tNorthEast.mY},
                               tWater);
                continue;
            }

            // Tum karoyu kaplayan en iyi doku (istenen seviyeye kadar; yoksa daha kabasi)
            const int tFullLevel = std::min(tLevel, 2);
            const Gfx::Texture* tTexture = nullptr;
            for (int tTry = tFullLevel; tTry >= 0 && !tTexture; --tTry)
            {
                tTexture = useTexture(keyOf(tLon, tLat, tTry, 0, tStyle, tAreas));
            }
            if (!useTexture(keyOf(tLon, tLat, tFullLevel, 0, tStyle, tAreas)))
            {
                mStore.Request(keyOf(tLon, tLat, tFullLevel, 0, tStyle, tAreas));
                ++mPendingThisFrame;
            }
            if (!useTexture(keyOf(tLon, tLat, 0, 0, tStyle, tAreas)))
            {
                mStore.Request(keyOf(tLon, tLat, 0, 0, tStyle, tAreas));  // en kaba seviye hep olsun: hizli yedek
            }
            if (tTexture)
            {
                tStrips.clear();
                addLatLonStrips(tStrips, tView, tTileWest, tLat, tTileEast, tLat + 1.0, kFullTileStrips);
                tCanvas.drawTextured(*tTexture, tStrips);
            }
            else if (tPlaceholder)
            {
                tLoading.addQuad(tSouthWest, {tNorthEast.mX, tSouthWest.mY}, tNorthEast,
                                 {tSouthWest.mX, tNorthEast.mY}, tLand);
                tHasLoading = true;
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
                    const Gfx::Texture* tChunkTexture = useTexture(keyOf(tLon, tLat, 3, tChunk, tStyle, tAreas));
                    if (!tChunkTexture)
                    {
                        mStore.Request(keyOf(tLon, tLat, 3, tChunk, tStyle, tAreas));
                        ++mPendingThisFrame;
                        continue;
                    }
                    tStrips.clear();
                    addLatLonStrips(tStrips, tView, tChunkWest, tChunkNorth - 0.25, tChunkWest + 0.25, tChunkNorth,
                                    kChunkStrips);
                    tCanvas.drawTextured(*tChunkTexture, tStrips);
                }
            }

            // Yakin zoom: ekrana giren 1/32 derecelik parcalar (DSF ucgenlerinden);
            // seviye 5'te de altta yedek olarak cizilir
            if (tLevel >= TerrainStore::kDetailLevel)
            {
                drawDetailChunks(tCanvas, tView, tBounds, TerrainStore::kDetailLevel, tLon, tLat, tTileWest, tStyle,
                                 tAreas, tStrips);
            }
            // Cok yakin zoom: 1/128 derecelik parcalar
            if (tLevel == TerrainStore::kFineLevel)
            {
                drawDetailChunks(tCanvas, tView, tBounds, TerrainStore::kFineLevel, tLon, tLat, tTileWest, tStyle,
                                 tAreas, tStrips);
            }
        }
    }
    tCanvas.draw(tOcean);
    tCanvas.draw(tNoData);
    if (tHasLoading)
    {
        tCanvas.draw(tLoading);
    }
    mHub.evictUnusedTextures();
}
