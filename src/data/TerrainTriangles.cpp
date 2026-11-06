// TerrainTriangles.cpp
#include "data/TerrainTriangles.h"

#include <algorithm>
#include <cmath>

namespace
{

int ClampChunk(int pValue)
{
    return std::min(std::max(pValue, 0), TerrainTriangles::kChunksPerSide - 1);
}

}  // namespace

void TerrainTriangles::Reset(int pWest, int pSouth)
{
    mWest = pWest;
    mSouth = pSouth;
    mCoords.clear();
    mClasses.clear();
    mChunks.assign(static_cast<size_t>(kChunksPerSide) * kChunksPerSide, {});
}

int TerrainTriangles::West() const
{
    return mWest;
}

int TerrainTriangles::South() const
{
    return mSouth;
}

void TerrainTriangles::Add(const double pLon[3], const double pLat[3], TerrainClass pClass)
{
    const uint32_t tIndex = static_cast<uint32_t>(mClasses.size());
    double tMinX = 1e9;
    double tMaxX = -1e9;
    double tMinRow = 1e9;
    double tMaxRow = -1e9;
    for (int tCorner = 0; tCorner < 3; ++tCorner)
    {
        // float: karo icinde 1e-7 derece (~1 cm) hassasiyet yeterli
        const double tDLon = pLon[tCorner] - mWest;
        const double tDLat = pLat[tCorner] - mSouth;
        mCoords.push_back(static_cast<float>(tDLon));
        mCoords.push_back(static_cast<float>(tDLat));
        tMinX = std::min(tMinX, tDLon);
        tMaxX = std::max(tMaxX, tDLon);
        tMinRow = std::min(tMinRow, 1.0 - tDLat);  // satir 0 = kuzey
        tMaxRow = std::max(tMaxRow, 1.0 - tDLat);
    }
    mClasses.push_back(static_cast<uint8_t>(pClass));

    const int tFirstColumn = ClampChunk(static_cast<int>(std::floor(tMinX * kChunksPerSide)));
    const int tLastColumn = ClampChunk(static_cast<int>(std::floor(tMaxX * kChunksPerSide)));
    const int tFirstRow = ClampChunk(static_cast<int>(std::floor(tMinRow * kChunksPerSide)));
    const int tLastRow = ClampChunk(static_cast<int>(std::floor(tMaxRow * kChunksPerSide)));
    for (int tRow = tFirstRow; tRow <= tLastRow; ++tRow)
    {
        for (int tColumn = tFirstColumn; tColumn <= tLastColumn; ++tColumn)
        {
            mChunks[static_cast<size_t>(tRow) * kChunksPerSide + tColumn].push_back(tIndex);
        }
    }
}

size_t TerrainTriangles::TriangleCount() const
{
    return mClasses.size();
}

size_t TerrainTriangles::MemoryBytes() const
{
    size_t tBytes = mCoords.capacity() * sizeof(float) + mClasses.capacity();
    for (const std::vector<uint32_t>& tChunk : mChunks)
    {
        tBytes += tChunk.capacity() * sizeof(uint32_t);
    }
    return tBytes;
}

const std::vector<uint32_t>& TerrainTriangles::TrianglesIn(int pChunk) const
{
    return mChunks[static_cast<size_t>(pChunk)];
}

void TerrainTriangles::Corners(uint32_t pIndex, double pLon[3], double pLat[3]) const
{
    const float* tCoords = mCoords.data() + static_cast<size_t>(pIndex) * 6;
    for (int tCorner = 0; tCorner < 3; ++tCorner)
    {
        pLon[tCorner] = tCoords[tCorner * 2];
        pLat[tCorner] = tCoords[tCorner * 2 + 1];
    }
}

TerrainClass TerrainTriangles::ClassOf(uint32_t pIndex) const
{
    return static_cast<TerrainClass>(mClasses[pIndex]);
}
