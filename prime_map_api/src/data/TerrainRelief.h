// Zemin piksel formulleri (saf fonksiyonlar, X-Plane/openGL bagimsiz, testli).
// Kabartmada cok yonlu isik kullanilir: tek yonlu isik bazi yamaclari dumduz birakir
// ve ters kabartma yanilsamasi yaratir. VFR bandi sinirinda yarim pikselden yakin
// pikseller iki bandin karisimidir (kenar yumusatma).
#ifndef PRIME_MAP_SRC_DATA_TERRAINRELIEF_H
#define PRIME_MAP_SRC_DATA_TERRAINRELIEF_H

#include <cstdint>
#include <vector>

#include "data/TerrainClasses.h"

// mExaggeration * (pCellMeters / pFinestCellMeters) ^ mCoarseBoost
double reliefExaggeration(const TerrainReliefParams& pParams, double pCellMeters, double pFinestCellMeters);

// Kabartma parlaklik carpani (0.45..1.55). Egimler metre/metre, abartmasiz.
// mFlatSlope altindaki egimler bastirilir: DEM'in 1 m basamaklari dalga gibi gorunmesin.
double reliefFactor(double pSlopeEast, double pSlopeNorth, double pStrength, double pExaggeration,
                    const TerrainReliefParams& pParams);

// VFR bandi rengi (0..255). pFeetPerPixel: rakimin piksel basina degisimi; 0 ise yumusatma yok.
void vfrBandColor(double pMeters, double pFeetPerPixel, const TerrainVfrParams& pParams, double pRgb[3]);

// Derinlik koyulasma orani 0..1. -10 m ve ustu (veri yok, gol, kiyi) 0.
double depthDarkening(double pSeaLevel, const TerrainWaterParams& pParams);

// Kiyi seridi acikligi 0..1: cizgiden uzaklastikca karesel azalir.
double coastBandLightening(double pDistancePixels, double pBandPixels, const TerrainWaterParams& pParams);

// Her pikselin en yakin kara pikseline uzakligi (piksel; 3-4 chamfer, ~%8 hata).
void distanceToLand(const std::vector<uint8_t>& pLand, int pSize, float pMaxDistance, std::vector<float>* pOut);

// Kiyi cizgisi gucu 0..1; suyun tarafinda tek piksel (4 komsusundan biri kara olan su pikseli).
void coastlineStrength(const std::vector<float>& pLandFraction, int pSize, std::vector<float>* pOut);
#endif  // PRIME_MAP_SRC_DATA_TERRAINRELIEF_H
