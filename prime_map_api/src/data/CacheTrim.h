// Zemin disk onbellegini boyut sinirinda tutar; en eski karolari ve kalan gecici dosyalari siler.
#ifndef PRIME_MAP_SRC_DATA_CACHETRIM_H
#define PRIME_MAP_SRC_DATA_CACHETRIM_H

#include <cstdint>
#include <string>

struct CacheTrimResult
{
    uint64_t mTotalBytes = 0;
    uint64_t mRemovedBytes = 0;
    int mRemovedTiles = 0;
    int mRemovedTempFiles = 0;
};

void touchCacheFile(const std::string& pPath);

CacheTrimResult trimTileCache(const std::string& pDirectory, uint64_t pMaxBytes);
#endif
