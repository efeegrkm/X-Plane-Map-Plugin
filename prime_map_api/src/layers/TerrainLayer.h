// Zemin: X-Plane sahnesinin (DSF) arazisi; su, sehir, orman, kabartma golgesi.
// Dokular haritalarin ORTAK TerrainHub'inda durur; palet (zemin ayarlari)
// harita basinasdir. Ince seviye hazir olana kadar en iyi kaba seviye gosterilir.
#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Layer.h"
#include "MapView.h"
#include "gfx/Mesh.h"
#include "gfx/Texture.h"
#include "data/TerrainClasses.h"
#include "data/TerrainStore.h"
#include "layers/TerrainHub.h"

class TerrainLayer : public Layer
{
public:
    // pPaletteId: paletin TerrainStore'daki numarasi (TerrainStore::addPalette; Start'a verilen 0)
    TerrainLayer(TerrainHub& pHub, const TerrainPalette& pPalette, int pPaletteId = 0);
    ~TerrainLayer() override;

    // Bu haritanin zemin ayarlari degisti: yeni dokular bu paletle istenir
    void setPalette(const TerrainPalette& pPalette, int pPaletteId);
    int paletteId() const;

    void draw(const drawContext& pContext) override;
    bool prepareFrame() override;

    int textureCount() const;

    // "Loading terrain" yazisi icin: kaba yedek ya da bos gorunen her an > 0
    int pendingTextureCount() const;

private:
    const Gfx::Texture* useTexture(const TerrainImageKey& pKey);
    static int desiredLevel(double pPixelsPerDegree);
    // pLevel: TerrainStore::kDetailLevel (1/32 derece) veya kFineLevel (1/128 derece)
    void drawDetailChunks(Gfx::Canvas& pCanvas, const XMapView& pView, const GeoBounds& pBounds, int pLevel, int pLon,
                          int pLat, double pTileWest, int pStyle, int pAreas,
                          std::vector<Gfx::TexturedVertex>& pStrips);
    // Anahtar: bu haritanin palet numarasiyla
    TerrainImageKey keyOf(int pLon, int pLat, int pLevel, int pChunk, int pStyle, int pAreas) const;

    int mForestPart = -1;   // alt katman: DSF orman poligonlari (Scenery)
    int mBuiltUpPart = -1;  // alt katman: yerlesim alanlari (Scenery: sinif rengi, VFR: sari)
    TerrainHub& mHub;        // ortak dokular (haritalar arasi)
    TerrainStore& mStore;    // mHub.store(): istekler ve karo durumu
    TerrainPalette mPalette;
    int mPaletteId = 0;
    int mUser = 0;           // bu haritanin TerrainHub numarasi
    uint64_t mSeenGeneration = 0;
    int mPendingThisFrame = 0;
    int mPendingLastFrame = 0;
};
