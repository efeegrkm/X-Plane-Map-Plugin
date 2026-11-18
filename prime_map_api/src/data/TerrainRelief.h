// Zemin piksel formulleri: kabartma golgesi, yukseklik renkleri ve su tonlari.
#ifndef PRIME_MAP_SRC_DATA_TERRAINRELIEF_H
#define PRIME_MAP_SRC_DATA_TERRAINRELIEF_H

#include <cstdint>
#include <vector>

#include "data/TerrainClasses.h"

double reliefExaggeration(const TerrainReliefParams& pParams, double pCellMeters, double pFinestCellMeters);

double reliefFactor(double pSlopeEast, double pSlopeNorth, double pStrength, double pExaggeration,
                    const TerrainReliefParams& pParams);

void vfrBandColor(double pMeters, double pFeetPerPixel, const TerrainVfrParams& pParams, double pRgb[3]);

double depthDarkening(double pSeaLevel, const TerrainWaterParams& pParams);

double coastBandLightening(double pDistancePixels, double pBandPixels, const TerrainWaterParams& pParams);

void distanceToLand(const std::vector<uint8_t>& pLand, int pSize, float pMaxDistance, std::vector<float>* pOut);

void coastlineStrength(const std::vector<float>& pLandFraction, int pSize, std::vector<float>* pOut);
#endif
