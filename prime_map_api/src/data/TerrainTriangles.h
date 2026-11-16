// Yakin zoom icin bir 1x1 derece karonun temel arazi ucgenleri, hafif bicimde.
// X-Plane'e bagimli DEGIL; arka plan thread'inde doldurulur. Neden: sinif
// raster'i yaklasinca bloklasir, DSF ucgenleri keskin kalir (TerrainRaster::buildDetailImage).
//
// Bellek: ucgen basina ~29 bayt; 1 milyon ucgenlik karo ~30 MB. TerrainStore
// en fazla 2 karo tutar.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "data/TerrainClasses.h"

class TerrainTriangles
{
public:
    static const int kChunksPerSide = 32;  // parca = 1/32 derece (~3,5 km enlem)

    // Karoyu bosaltir; pWest/pSouth: guney-bati kose
    void reset(int pWest, int pSouth);

    int west() const;
    int south() const;

    // Ucgeni ekler ve kapsayan kutusunun degdigi her parcaya kaydeder
    void add(const double pLon[3], const double pLat[3], TerrainClass pClass);

    size_t triangleCount() const;
    size_t memoryBytes() const;

    // pChunk: satir * kChunksPerSide + sutun (satir 0 = KUZEY, sutun 0 = BATI)
    const std::vector<uint32_t>& trianglesIn(int pChunk) const;

    // pIndex. ucgenin kose koordinatlari: karonun bati/guney kosesine gore derece
    void corners(uint32_t pIndex, double pLon[3], double pLat[3]) const;
    TerrainClass classOf(uint32_t pIndex) const;

private:
    int mWest = 0;
    int mSouth = 0;
    std::vector<float> mCoords;     // ucgen basina 6 float: (dlon, dlat) x 3
    std::vector<uint8_t> mClasses;  // ucgen basina sinif
    std::vector<std::vector<uint32_t>> mChunks;  // kChunksPerSide^2 liste
};
