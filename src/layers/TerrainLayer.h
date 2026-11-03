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
//                                 daha cok -> seviye 3 (4096 px, 16 parca)
// Ince seviye hazir olana kadar elde olan en iyi kaba seviye gosterilir.
// Deniz karolari (DSF yok, bolge kurulu) su rengiyle doldurulur.
#pragma once

#include <cstdint>
#include <unordered_map>

#include "Layer.h"
#include "data/TerrainClasses.h"
#include "data/TerrainStore.h"

class TerrainLayer : public Layer
{
public:
    TerrainLayer(TerrainStore& pStore, const TerrainPalette& pPalette);
    ~TerrainLayer() override;

    void Draw(const DrawContext& pContext) override;

    int TextureCount() const;

private:
    struct CachedTexture
    {
        int mTexture = 0;
        size_t mBytes = 0;
        uint64_t mLastFrame = 0;
    };

    void UploadReadyImages();
    int UseTexture(const TerrainImageKey& pKey);  // varsa doku numarasi (kullanildi olarak isaretler), yoksa 0
    void EvictUnused();
    static int DesiredLevel(double pPixelsPerDegree);

    TerrainStore& mStore;
    TerrainPalette mPalette;
    std::unordered_map<TerrainImageKey, CachedTexture, TerrainImageKeyHash> mTextures;
    size_t mTextureBytes = 0;
    uint64_t mFrame = 0;
};
