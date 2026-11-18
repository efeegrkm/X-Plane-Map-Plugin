// X-Plane'in dunya karo tablosu: hangi karoda sahne bulundugunu soyler.
#ifndef PRIME_MAP_SRC_DATA_WORLDTILEMAP_H
#define PRIME_MAP_SRC_DATA_WORLDTILEMAP_H

#include <cstdint>
#include <string>
#include <vector>

class WorldTileMap
{
public:
    static const size_t kHeaderSize = 5;
    static const size_t kFileSize = kHeaderSize + 360 * 180;

    bool load(const std::string& pPath);
    bool loadBytes(const std::vector<uint8_t>& pBytes);

    bool isLoaded() const;
    void clear();

    bool hasDsf(int pWest, int pSouth) const;
    int landTileCount() const;

    static size_t indexOf(int pWest, int pSouth);

private:
    std::vector<uint8_t> mTiles;
};
#endif
