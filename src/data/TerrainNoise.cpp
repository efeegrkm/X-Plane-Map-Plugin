// TerrainNoise.cpp
#include "data/TerrainNoise.h"

#include <algorithm>
#include <cmath>

namespace
{

const double kMetersPerDegree = 111320.0;
const double kLacunarity = 2.3;  // oktavlar arasi frekans carpani (2 yerine: duzenli izgara izi olmasin)
const double kGain = 0.5;
const uint32_t kSeedEast = 0x5EEDu;
const uint32_t kSeedNorth = 0xB0A7u;

// Tam sayi izgara noktasinin [-1, 1] degeri (splitmix64 benzeri karistirma)
double LatticeValue(int64_t pX, int64_t pY, uint32_t pSeed)
{
    uint64_t tHash = static_cast<uint64_t>(pX) * 0x9E3779B97F4A7C15ull ^
                     static_cast<uint64_t>(pY) * 0xC2B2AE3D27D4EB4Full ^
                     static_cast<uint64_t>(pSeed) * 0x165667B19E3779F9ull;
    tHash ^= tHash >> 33;
    tHash *= 0xFF51AFD7ED558CCDull;
    tHash ^= tHash >> 33;
    tHash *= 0xC4CEB9FE1A85EC53ull;
    tHash ^= tHash >> 33;
    return static_cast<double>(tHash >> 11) * (2.0 / 9007199254740992.0) - 1.0;  // 53 bit -> [-1, 1)
}

double Fade(double pT)
{
    return pT * pT * pT * (pT * (pT * 6.0 - 15.0) + 10.0);  // 5. derece: turev surekli, izgara izi yok
}

double ValueNoise(double pX, double pY, uint32_t pSeed)
{
    const double tFloorX = std::floor(pX);
    const double tFloorY = std::floor(pY);
    const int64_t tX = static_cast<int64_t>(tFloorX);
    const int64_t tY = static_cast<int64_t>(tFloorY);
    const double tU = Fade(pX - tFloorX);
    const double tV = Fade(pY - tFloorY);
    const double tA = LatticeValue(tX, tY, pSeed);
    const double tB = LatticeValue(tX + 1, tY, pSeed);
    const double tC = LatticeValue(tX, tY + 1, pSeed);
    const double tD = LatticeValue(tX + 1, tY + 1, pSeed);
    return (tA + (tB - tA) * tU) + ((tC + (tD - tC) * tU) - (tA + (tB - tA) * tU)) * tV;
}

}  // namespace

double TerrainNoise(double pX, double pY, uint32_t pSeed, int pOctaves)
{
    double tSum = 0.0;
    double tNorm = 0.0;
    double tAmplitude = 1.0;
    double tFrequency = 1.0;
    for (int tOctave = 0; tOctave < pOctaves; ++tOctave)
    {
        // Her oktav kaydirilmis: oktavlarin izgara noktalari ust uste binmesin
        tSum += tAmplitude * ValueNoise(pX * tFrequency + tOctave * 17.31, pY * tFrequency - tOctave * 9.77,
                                        pSeed + static_cast<uint32_t>(tOctave) * 101u);
        tNorm += tAmplitude;
        tAmplitude *= kGain;
        tFrequency *= kLacunarity;
    }
    return tNorm > 0.0 ? tSum / tNorm : 0.0;
}

void TerrainWarpOffset(double pLon, double pLat, const TerrainWarpParams& pParams, double* pEastMeters,
                       double* pNorthMeters)
{
    *pEastMeters = 0.0;
    *pNorthMeters = 0.0;
    if (pParams.mAmplitudeMeters <= 0.0 || pParams.mWavelengthMeters <= 0.0)
    {
        return;
    }
    // Metre benzeri duzlem: dogu = boylam * cos(enlem) (noktanin KENDI enlemi: karo sinirlarinda surekli)
    const double tX = pLon * kMetersPerDegree * std::cos(pLat * 3.14159265358979323846 / 180.0) /
                      pParams.mWavelengthMeters;
    const double tY = pLat * kMetersPerDegree / pParams.mWavelengthMeters;
    *pEastMeters = TerrainNoise(tX, tY, kSeedEast, pParams.mOctaves) * pParams.mAmplitudeMeters;
    *pNorthMeters = TerrainNoise(tX, tY, kSeedNorth, pParams.mOctaves) * pParams.mAmplitudeMeters;
}

TerrainWarpPlan TerrainPlanWarp(const TerrainWarpParams& pParams, double pMetersPerPixelX, double pMetersPerPixelY)
{
    TerrainWarpPlan tPlan;
    if (pParams.mAmplitudeMeters <= 0.0 || pMetersPerPixelX <= 0.0 || pMetersPerPixelY <= 0.0)
    {
        return tPlan;
    }
    if (pParams.mAmplitudeMeters < 0.75 * std::max(pMetersPerPixelX, pMetersPerPixelY))
    {
        return tPlan;
    }
    tPlan.mActive = true;
    const double tMinPixel = std::min(pMetersPerPixelX, pMetersPerPixelY);
    tPlan.mPad = static_cast<int>(std::ceil(pParams.mAmplitudeMeters / tMinPixel)) + 1;
    // En ince oktavin dalga boyu / 6 piksel adim yeterli (alan yumusak); 1..16
    const double tFinest = pParams.mWavelengthMeters / std::pow(kLacunarity, pParams.mOctaves - 1);
    tPlan.mStep = std::clamp(static_cast<int>(tFinest / tMinPixel / 6.0), 1, 16);
    return tPlan;
}
