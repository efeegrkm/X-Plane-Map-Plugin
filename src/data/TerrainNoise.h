// TerrainNoise.h
// Zemin dokusunda arazi sinifi sinirlarini dogal gostermek icin cografi koordinata
// bagli, deterministik yumusak gurultu (deger gurultusu + fBm). X-Plane'e bagimli DEGIL.
//
// Neden: DSF arazi turu ucgen basina; yakin zoom'da duz kenarlar ve sivri koseler
// gorunur. Sinif okunurken bakilan nokta bu gurultuyle birkac on metre kaydirilir
// ("domain warp"): sinirlar kivrilir, zemin keskin kalir. Gurultu enlem/boylamdan
// uretildigi icin her seviyede ve her parcada ayni sonucu verir (dikis / sicrama yok).
#pragma once

#include <cstdint>

struct TerrainWarpParams
{
    double mAmplitudeMeters = 65.0;    // en buyuk kayma (0 = kapali)
    double mWavelengthMeters = 250.0;  // en kaba oktavin dalga boyu
    int mOctaves = 3;
};

// Noktanin kayma vektoru (metre; dogu ve kuzey). |kayma| <= mAmplitudeMeters.
void TerrainWarpOffset(double pLon, double pLat, const TerrainWarpParams& pParams, double* pEastMeters,
                       double* pNorthMeters);

// Test ve ayar icin: tek kanal fBm degeri, yaklasik [-1, 1]
double TerrainNoise(double pX, double pY, uint32_t pSeed, int pOctaves);

// Bukme plani: genlik pikselin ~0,75'inden kucukse etkisiz (uzak seviyeler)
struct TerrainWarpPlan
{
    bool mActive = false;
    int mPad = 0;   // cikti pikseli: en buyuk kayma + 1
    int mStep = 1;  // kayma alaninin hesaplandigi izgara adimi (piksel), arasi cift dogrusal
};

// pMetersPerPixelX/Y: dokunun pikselinin yerdeki boyu (metre). Stiller arasindan Scenery
// (Classes) bukmeye uygundur; stil kontrolunu cagiran taraf yapar.
TerrainWarpPlan TerrainPlanWarp(const TerrainWarpParams& pParams, double pMetersPerPixelX,
                                double pMetersPerPixelY);
