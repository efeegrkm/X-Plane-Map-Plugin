// Arazi sinirlarini dogal gostermek icin konumdan uretilen belirlenimci gurultu.
#ifndef PRIME_MAP_SRC_DATA_TERRAINNOISE_H
#define PRIME_MAP_SRC_DATA_TERRAINNOISE_H

#include <cstdint>

struct TerrainWarpParams
{
    double mAmplitudeMeters = 65.0;
    double mWavelengthMeters = 250.0;
    int mOctaves = 3;
};

void terrainWarpOffset(double pLon, double pLat, const TerrainWarpParams& pParams, double* pEastMeters,
                       double* pNorthMeters);

double TerrainNoise(double pX, double pY, uint32_t pSeed, int pOctaves);

struct TerrainWarpPlan
{
    bool mActive = false;
    int mPad = 0;
    int mStep = 1;
};

TerrainWarpPlan terrainPlanWarp(const TerrainWarpParams& pParams, double pMetersPerPixelX,
                                double pMetersPerPixelY);
#endif
