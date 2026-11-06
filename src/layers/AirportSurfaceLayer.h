// AirportSurfaceLayer.h
// Havalimani yuzeyleri: apron / taksi yolu (110), havalimani siniri (130),
// boya cizgileri (120 ve yuzey kenarlari). Veri AirportDetailStore'dan
// gelir; ekranda olup ayrintisi henuz okunmamis havalimaninin yerinde
// yanip sonen kesikli bir kutu gorunur.
//
// Performans: bir havalimaninin sekilleri ilk gorundugunde BIR KEZ GPU'ya
// yuklenir (Gfx::StaticMesh, Mercator koordinatinda). Sonraki karelerde
// sadece olcek + kaydirma degisir; nokta basina CPU isi yok.
//
// Zoom esikleri:
//   10.5+ sinir cizgisi, 11+ yuzeyler, 13.5+ boya cizgileri
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

    void Draw(const DrawContext& pContext) override;
    std::string Describe(const DrawContext& pContext, double pX, double pY) const override;

private:
    // Bir havalimaninin GPU'daki sekilleri
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

    void Upload(GpuAirport& pGpu, const LoadedAirport& pLoaded) const;
    void EvictUnused();

    const AirportGeometryStore& mSummaries;
    AirportDetailStore& mDetails;
    std::vector<int> mQuery;
    std::vector<int> mVisible;
    std::unordered_map<int, GpuAirport> mGpu;
    std::vector<const LoadedAirport*> mDrawn;  // son karede cizilenler (merkeze yakin once)
    uint64_t mFrame = 0;
};
