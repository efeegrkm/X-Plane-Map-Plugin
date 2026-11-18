// DSF arazisinden uretilen zemin dokularinin cizimi.
#ifndef PRIME_MAP_SRC_LAYERS_TERRAINLAYER_H
#define PRIME_MAP_SRC_LAYERS_TERRAINLAYER_H

#include <cstdint>
#include <vector>

#include "Layer.h"
#include "XMapView.h"
#include "gfx/Mesh.h"
#include "gfx/Texture.h"
#include "data/TerrainClasses.h"
#include "data/TerrainStore.h"
#include "layers/TerrainHub.h"

class TerrainLayer : public Layer
{
public:
    TerrainLayer(TerrainHub& pHub, const TerrainPalette& pPalette, int pPaletteId = 0);
    ~TerrainLayer() override;

    void setPalette(const TerrainPalette& pPalette, int pPaletteId);
    int paletteId() const;

    void draw(const drawContext& pContext) override;
    bool prepareFrame() override;

    int textureCount() const;

    int pendingTextureCount() const;

private:
    const Gfx::Texture* useTexture(const TerrainImageKey& pKey);
    static int desiredLevel(double pPixelsPerDegree);
    void drawDetailChunks(Gfx::Canvas& pCanvas, const XMapView& pView, const GeoBounds& pBounds, int pLevel, int pLon,
                          int pLat, double pTileWest, int pStyle, int pAreas,
                          std::vector<Gfx::TexturedVertex>& pStrips);
    TerrainImageKey keyOf(int pLon, int pLat, int pLevel, int pChunk, int pStyle, int pAreas) const;

    int mForestPart = -1;
    int mBuiltUpPart = -1;
    TerrainHub& mHub;
    TerrainStore& mStore;
    TerrainPalette mPalette;
    int mPaletteId = 0;
    int mUser = 0;
    uint64_t mSeenGeneration = 0;
    int mPendingThisFrame = 0;
    int mPendingLastFrame = 0;
};
#endif
