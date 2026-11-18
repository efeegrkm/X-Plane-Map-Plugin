// Arazi sinifi renklerini X-Plane'in arazi dokularindan hesaplar.
#ifndef PRIME_MAP_SRC_DATA_TERRAINPALETTESOURCE_H
#define PRIME_MAP_SRC_DATA_TERRAINPALETTESOURCE_H

#include <array>
#include <string>

#include "data/TerrainClasses.h"

struct XPlaneClassColors
{
    std::array<TerrainColor, kTerrainClassCount> mColors = {};
    std::array<int, kTerrainClassCount> mTextureCounts = {};
};

bool loadXPlaneClassColors(const std::string& pXPlaneDirectory, const std::string& pCachePath,
                           XPlaneClassColors* pOut, std::string* pLog);

bool averageDdsColor(const std::string& pPath, TerrainColor* pOut);
#endif
