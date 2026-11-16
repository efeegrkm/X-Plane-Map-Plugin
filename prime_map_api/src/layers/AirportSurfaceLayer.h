// Havalimani yuzeyleri: apron / taksi yolu, havalimani siniri, boya cizgileri.
// Sekiller ilk gorundugunde BIR KEZ GPU'ya yuklenir (Gfx::StaticMesh,
// Mercator); sonraki karelerde sadece olcek + kaydirma degisir.
// Ayrintisi henuz okunmamis havalimaninda kesikli kutu gorunur.
#pragma once

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
        const LoadedAirport* mSource = nullptr;  // hangi ayrintidan uretildi
        double mOriginX = 0.0;                   // Mercator
        double mOriginY = 0.0;
        Gfx::StaticMesh mFills;                  // yuzey dolgulari (ucgen)
        Gfx::StaticMesh mEdges;                  // yuzey kenarlari (cizgi)
        Gfx::StaticMesh mBoundaries;             // havalimani siniri (cizgi)
        Gfx::StaticMesh mLines;                  // boya cizgileri (cizgi)
        uint64_t mLastFrame = 0;
    };

    void upload(GpuAirport& pGpu, const LoadedAirport& pLoaded) const;
    void evictUnused();

    const AirportGeometryStore& mSummaries;
    AirportDetailStore& mDetails;
    std::vector<int> mQuery;
    std::vector<int> mVisible;
    std::unordered_map<int, GpuAirport> mGpu;
    std::vector<const LoadedAirport*> mDrawn;  // son karede cizilenler (merkeze yakin once)
    uint64_t mFrame = 0;
};
