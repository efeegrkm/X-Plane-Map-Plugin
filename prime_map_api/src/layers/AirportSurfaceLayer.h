// Havalimani yuzeyleri, sinirlari ve boya cizgileri.
#ifndef PRIME_MAP_SRC_LAYERS_AIRPORTSURFACELAYER_H
#define PRIME_MAP_SRC_LAYERS_AIRPORTSURFACELAYER_H

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Layer.h"
#include "gfx/StaticMesh.h"

class AirportGeometryStore;
class AirportDetailStore;
struct LoadedAirport;

class AirportSurfaceLayer : public Layer
{
public:
    AirportSurfaceLayer(const AirportGeometryStore& pSummaries, AirportDetailStore& pDetails);

    void draw(const drawContext& pContext) override;
    std::string describe(const drawContext& pContext, double pX, double pY) const override;

private:
    struct GpuAirport
    {
        const LoadedAirport* mSource = nullptr;
        double mOriginX = 0.0;
        double mOriginY = 0.0;
        Gfx::StaticMesh mFills;
        Gfx::StaticMesh mEdges;
        Gfx::StaticMesh mBoundaries;
        Gfx::StaticMesh mLines;
        uint64_t mLastFrame = 0;
    };

    void upload(GpuAirport& pGpu, const LoadedAirport& pLoaded) const;
    void evictUnused();

    const AirportGeometryStore& mSummaries;
    AirportDetailStore& mDetails;
    std::vector<int> mQuery;
    std::vector<int> mVisible;
    std::unordered_map<int, GpuAirport> mGpu;
    std::vector<const LoadedAirport*> mDrawn;
    uint64_t mFrame = 0;
};
#endif
