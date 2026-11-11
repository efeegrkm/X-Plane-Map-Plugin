// WorldTileMap.cpp
#include "data/WorldTileMap.h"

#include <fstream>
#include <iterator>

bool WorldTileMap::Load(const std::string& pPath)
{
    std::ifstream tFile(pPath, std::ios::binary);
    if (!tFile)
    {
        Clear();
        return false;
    }
    const std::vector<uint8_t> tBytes((std::istreambuf_iterator<char>(tFile)), std::istreambuf_iterator<char>());
    return LoadBytes(tBytes);
}

bool WorldTileMap::LoadBytes(const std::vector<uint8_t>& pBytes)
{
    if (pBytes.size() != kFileSize)
    {
        Clear();  // beklenmeyen bicim: tahmin etme, kullanma
        return false;
    }
    mTiles.assign(pBytes.begin() + kHeaderSize, pBytes.end());
    return true;
}

bool WorldTileMap::IsLoaded() const
{
    return !mTiles.empty();
}

void WorldTileMap::Clear()
{
    mTiles.clear();
}

size_t WorldTileMap::IndexOf(int pWest, int pSouth)
{
    return static_cast<size_t>(pWest + 180) * 180 + static_cast<size_t>(pSouth + 90);
}

bool WorldTileMap::HasDsf(int pWest, int pSouth) const
{
    if (mTiles.empty() || pWest < -180 || pWest > 179 || pSouth < -90 || pSouth > 89)
    {
        return false;
    }
    return mTiles[IndexOf(pWest, pSouth)] != 0;
}

int WorldTileMap::LandTileCount() const
{
    int tCount = 0;
    for (const uint8_t tValue : mTiles)
    {
        tCount += tValue != 0 ? 1 : 0;
    }
    return tCount;
}
