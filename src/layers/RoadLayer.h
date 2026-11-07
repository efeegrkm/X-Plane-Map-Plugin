// RoadLayer.h
// Asama 2: DSF yol agi (otoyol, ana yol, tali yol, yerel yol, demiryolu, enerji
// hatti). Veri TerrainStore'dan karo karo gelir (TerrainVectors, RequestVectors);
// her karonun cizgileri bir kez GPU'ya yuklenir (StaticMesh, Mercator'da karo
// orijinine gore), her karede sadece olcek + kaydirma ile cizilir.
//
// Alt katmanlar ve zoom esikleri (asagidaki zoom'dan itibaren gorunur):
//   motorways 7.5   primary 9   secondary 10.5   local 12   railways 9.5
//   power_lines 11 (varsayilan kapali)
// Renkler temadan (MapTheme::mRoads); kalinliklar ekran pikseli (tur basina sabit).
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "Layer.h"
#include "data/TerrainVectors.h"
#include "gfx/StaticMesh.h"

class TerrainStore;

class RoadLayer : public Layer
{
public:
    explicit RoadLayer(TerrainStore& pStore);

    void Draw(const DrawContext& pContext) override;
    bool PrepareFrame() override;  // hazir vektorleri alir; aldiysa true

private:
    struct GpuTile
    {
        int mWest = 0;
        int mSouth = 0;
        std::shared_ptr<const TerrainVectors> mVectors;
        std::array<Gfx::StaticMesh, kVectorLineKindCount> mMeshes;
        const MapTheme* mUploadedTheme = nullptr;  // renk koselerde: tema degisince yeniden yuklenir
        double mOriginX = 0.0;  // karonun guney-bati kosesinin Mercator koordinati
        double mOriginY = 0.0;
        uint64_t mLastFrame = 0;
    };

    bool TakeReadyVectors();  // en az bir karo geldiyse true
    void Upload(GpuTile& pTile, const MapTheme& pTheme);
    void EvictUnused();

    TerrainStore& mStore;
    std::unordered_map<int, GpuTile> mTiles;  // SceneryIndex::TileKey
    std::array<int, kVectorLineKindCount> mParts = {};
    uint64_t mFrame = 0;
};
