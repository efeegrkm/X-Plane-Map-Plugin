// DSF yol agi (otoyol..demiryolu, enerji hatti). Veri TerrainStore'dan karo
// karo gelir; her karonun cizgileri bir kez GPU'ya yuklenir (Mercator'da karo
// orijinine gore), her karede sadece olcek + kaydirma ile cizilir.
// Renkler temadan; kalinliklar ekran pikseli (tur basina sabit).
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "Layer.h"
#include "data/TerrainVectors.h"
#include "gfx/StaticMesh.h"

class TerrainHub;

class RoadLayer : public Layer
{
public:
    explicit RoadLayer(TerrainHub& pHub);
    ~RoadLayer() override;

    void draw(const drawContext& pContext) override;
    bool prepareFrame() override;

private:
    struct GpuTile
    {
        int mWest = 0;
        int mSouth = 0;
        std::shared_ptr<const TerrainVectors> mVectors;
        std::array<Gfx::StaticMesh, kVectorLineKindCount> mMeshes;
        const XMapTheme* mUploadedTheme = nullptr;  // renk koselerde: tema degisince yeniden yuklenir
        double mOriginX = 0.0;  // karonun guney-bati kosesinin Mercator koordinati
        double mOriginY = 0.0;
        uint64_t mLastFrame = 0;
    };

    void upload(GpuTile& pTile, const XMapTheme& pTheme);
    void evictUnused();

    TerrainHub& mHub;
    int mUser = 0;                 // bu haritanin TerrainHub numarasi
    uint64_t mSeenGeneration = 0;  // en son gorulen TerrainHub::VectorGeneration
    std::unordered_map<int, GpuTile> mTiles;  // SceneryIndex::TileKey
    std::array<int, kVectorLineKindCount> mParts = {};
    uint64_t mFrame = 0;
};
