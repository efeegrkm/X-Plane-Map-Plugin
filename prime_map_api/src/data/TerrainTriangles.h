// Yakin zoom icin bir karonun arazi ucgenlerinin hafif kopyasi.
#ifndef PRIME_MAP_SRC_DATA_TERRAINTRIANGLES_H
#define PRIME_MAP_SRC_DATA_TERRAINTRIANGLES_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "data/TerrainClasses.h"

class TerrainTriangles
{
public:
    static const int kChunksPerSide = 32;

    void reset(int pWest, int pSouth);

    int west() const;
    int south() const;

    void add(const double pLon[3], const double pLat[3], TerrainClass pClass);

    size_t triangleCount() const;
    size_t memoryBytes() const;

    const std::vector<uint32_t>& trianglesIn(int pChunk) const;

    void corners(uint32_t pIndex, double pLon[3], double pLat[3]) const;
    TerrainClass classOf(uint32_t pIndex) const;

private:
    int mWest = 0;
    int mSouth = 0;
    std::vector<float> mCoords;
    std::vector<uint8_t> mClasses;
    std::vector<std::vector<uint32_t>> mChunks;
};
#endif
