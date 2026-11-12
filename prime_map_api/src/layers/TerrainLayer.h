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
//
// 0.23 (coklu harita): dokular bu katmanda degil, haritalarin ORTAK TerrainHub'inda durur
// (ayni yere bakan iki harita ayni dokuyu kullanir). Zemin ayarlari (palet) harita basinadir:
// SetPalette ile verilen palet numarasi isteklerin anahtarina girer; ayar degisince yeni
// numarayla yeni dokular istenir (X-Plane'i yeniden baslatmak gerekmez).
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
    // pPaletteId: paletin TerrainStore'daki numarasi (TerrainStore::AddPalette; Start'a verilen 0)
    TerrainLayer(TerrainHub& pHub, const TerrainPalette& pPalette, int pPaletteId = 0);
    ~TerrainLayer() override;

    // Bu haritanin zemin ayarlari degisti: yeni dokular bu paletle istenir
    void SetPalette(const TerrainPalette& pPalette, int pPaletteId);
    int PaletteId() const;

    void Draw(const DrawContext& pContext) override;
    bool PrepareFrame() override;  // hazir dokulari yukler; yuklediyse true

    int TextureCount() const;

    // Son cizilen karede ekranda istenen kalitede olmayan (henuz uretilmekte olan) doku sayisi.
    // Alt seritteki "Loading terrain" yazisi icin: kaba yedek ya da bos gorunen her an > 0.
    int PendingTextureCount() const;

private:
    // Varsa dokuyu (ortak TerrainHub'dan) verir ve bu harita icin isaretler; yoksa nullptr
    const Gfx::Texture* UseTexture(const TerrainImageKey& pKey);
    static int DesiredLevel(double pPixelsPerDegree);
    // Seviye 4: karonun ekrana giren 1/32 derecelik parcalarini ister ve cizer
    // pLevel: TerrainStore::kDetailLevel (1/32 derece) veya kFineLevel (1/128 derece)
    void DrawDetailChunks(Gfx::Canvas& pCanvas, const MapView& pView, const GeoBounds& pBounds, int pLevel, int pLon,
                          int pLat, double pTileWest, int pStyle, int pAreas,
                          std::vector<Gfx::TexturedVertex>& pStrips);
    // Anahtar: bu haritanin palet numarasiyla
    TerrainImageKey KeyOf(int pLon, int pLat, int pLevel, int pChunk, int pStyle, int pAreas) const;

    int mForestPart = -1;   // alt katman: DSF orman poligonlari (Scenery)
    int mBuiltUpPart = -1;  // alt katman: yerlesim alanlari (Scenery: sinif rengi, VFR: sari)
    TerrainHub& mHub;        // ortak dokular (haritalar arasi)
    TerrainStore& mStore;    // mHub.Store(): istekler ve karo durumu
    TerrainPalette mPalette;
    int mPaletteId = 0;
    int mUser = 0;           // bu haritanin TerrainHub numarasi
    uint64_t mSeenGeneration = 0;  // en son gorulen TerrainHub::TextureGeneration
    int mPendingThisFrame = 0;  // bu karede eksik bulunan (istenen) dokular
    int mPendingLastFrame = 0;
};
