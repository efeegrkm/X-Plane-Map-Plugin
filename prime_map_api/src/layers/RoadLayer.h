// DSF yol agi: otoyollar, yollar, demiryollari ve enerji hatlari.
#ifndef PRIME_MAP_SRC_LAYERS_ROADLAYER_H
#define PRIME_MAP_SRC_LAYERS_ROADLAYER_H

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
        const XMapTheme* mUploadedTheme = nullptr;
        double mOriginX = 0.0;
        double mOriginY = 0.0;
        uint64_t mLastFrame = 0;
    };

    void upload(GpuTile& pTile, const XMapTheme& pTheme);
    void evictUnused();

    TerrainHub& mHub;
    int mUser = 0;
    uint64_t mSeenGeneration = 0;
    std::unordered_map<int, GpuTile> mTiles;
    std::array<int, kVectorLineKindCount> mParts = {};
    uint64_t mFrame = 0;
};
#endif
