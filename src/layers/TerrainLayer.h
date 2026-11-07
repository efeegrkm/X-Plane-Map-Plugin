// TerrainLayer.h
// Zemin: bilgisayarda kurulu X-Plane sahnesinin (DSF) arazisi; su, sehir,
// orman, tarla ... ve kabartma golgesi. En alttaki katman.
//
// Goruntuler TerrainStore'dan (arka plan iscisi) gelir; bu katman sadece
// hangi karonun hangi cozunurlukte gerektigine karar verir, dokulari yukler
// ve cizer:
//   ekranda derece basina piksel  < 100  -> seviye 0 (64 px karo)
//                                 < 400  -> seviye 1 (256 px)
//                                 < 1600 -> seviye 2 (1024 px)
//                                 < 8000 -> seviye 3 (4096 px, 16 parca)
//                                 daha cok -> seviye 4 (DSF ucgenlerinden 1/32
//                                             derecelik parcalar, ~32768 px/derece)
// Ince seviye hazir olana kadar elde olan en iyi kaba seviye gosterilir.
// Deniz karolari su rengiyle, "veri yok" karolari (kara ama sahnesi kurulu degil)
// temanin notr mNoData rengiyle doldurulur (0.16; SceneryIndex, dsf_files).
// Komsu karonun rakimi henuz yokken uretilmis doku, komsu hazir olunca yeniden
// istenir ve gelince eskisinin yerine konur (karo sinirinda golge dikisi kalmaz).
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

class TerrainLayer : public Layer
{
public:
    TerrainLayer(TerrainStore& pStore, const TerrainPalette& pPalette);

    void Draw(const DrawContext& pContext) override;

    int TextureCount() const;

    // Son cizilen karede ekranda istenen kalitede olmayan (henuz uretilmekte olan) doku sayisi.
    // Alt seritteki "Loading terrain" yazisi icin: kaba yedek ya da bos gorunen her an > 0.
    int PendingTextureCount() const;

private:
    struct CachedTexture
    {
        Gfx::Texture mTexture;
        size_t mBytes = 0;
        uint64_t mLastFrame = 0;
        std::vector<int> mMissingNeighbors;  // uretilirken rakimi olmayan komsular (TerrainImage)
    };

    void UploadReadyImages();
    // Varsa dokuyu verir ve bu karede kullanildi diye isaretler; yoksa nullptr.
    // Eksik komsularindan biri artik hazirsa dokuyu yeniden ister (eskisi o gelene kadar kullanilir).
    const Gfx::Texture* UseTexture(const TerrainImageKey& pKey);
    void EvictUnused();
    static int DesiredLevel(double pPixelsPerDegree);
    // Seviye 4: karonun ekrana giren 1/32 derecelik parcalarini ister ve cizer
    // pLevel: TerrainStore::kDetailLevel (1/32 derece) veya kFineLevel (1/128 derece)
    void DrawDetailChunks(Gfx::Canvas& pCanvas, const MapView& pView, const GeoBounds& pBounds, int pLevel, int pLon,
                          int pLat, double pTileWest, int pStyle, int pAreas,
                          std::vector<Gfx::TexturedVertex>& pStrips);

    int mForestPart = -1;   // alt katman: DSF orman poligonlari (Scenery)
    int mBuiltUpPart = -1;  // alt katman: yerlesim alanlari (Scenery: sinif rengi, VFR: sari)
    TerrainStore& mStore;
    TerrainPalette mPalette;
    std::unordered_map<TerrainImageKey, CachedTexture, TerrainImageKeyHash> mTextures;
    size_t mTextureBytes = 0;
    uint64_t mFrame = 0;
    int mPendingThisFrame = 0;  // bu karede eksik bulunan (istenen) dokular
    int mPendingLastFrame = 0;
};
