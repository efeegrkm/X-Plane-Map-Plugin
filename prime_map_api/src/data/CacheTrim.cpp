// CacheTrim.cpp
#include "data/CacheTrim.h"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace
{

namespace fs = std::filesystem;

// Silme durunca toplam bu orana iner (sinirin hemen altinda her karoda yeniden silinmesin)
const double kTrimTarget = 0.9;

struct TileFiles
{
    std::vector<fs::path> mPaths;
    uint64_t mBytes = 0;
    fs::file_time_type mNewest = fs::file_time_type::min();  // karonun en son kullanilan dosyasi
};

// "+47-123" bicimi mi (SceneryIndex::TileName)
bool IsTileStem(const std::string& pStem)
{
    if (pStem.size() != 7)
    {
        return false;
    }
    for (size_t tIndex = 0; tIndex < pStem.size(); ++tIndex)
    {
        const char tChar = pStem[tIndex];
        const bool tSign = tChar == '+' || tChar == '-';
        if ((tIndex == 0 || tIndex == 3) ? !tSign : (tChar < '0' || tChar > '9'))
        {
            return false;
        }
    }
    return true;
}

}  // namespace

void TouchCacheFile(const std::string& pPath)
{
    std::error_code tError;
    fs::last_write_time(pPath, fs::file_time_type::clock::now(), tError);
}

CacheTrimResult TrimTileCache(const std::string& pDirectory, uint64_t pMaxBytes)
{
    CacheTrimResult tResult;
    std::error_code tError;
    std::unordered_map<std::string, TileFiles> tTiles;
    for (fs::directory_iterator tIterator(pDirectory, tError), tEnd; !tError && tIterator != tEnd;
         tIterator.increment(tError))
    {
        if (!tIterator->is_regular_file(tError))
        {
            continue;
        }
        // Sadece karo adli dosyalar ("+47-123.<uzanti>")
        const std::string tStem = tIterator->path().stem().string();
        if (!IsTileStem(tStem))
        {
            continue;
        }
        const uint64_t tSize = tIterator->file_size(tError);
        const fs::file_time_type tTime = tIterator->last_write_time(tError);
        if (tError)
        {
            tError.clear();
            continue;
        }
        TileFiles& tTile = tTiles[tStem];
        tTile.mPaths.push_back(tIterator->path());
        tTile.mBytes += tSize;
        tTile.mNewest = std::max(tTile.mNewest, tTime);
        tResult.mTotalBytes += tSize;
    }
    if (pMaxBytes == 0 || tResult.mTotalBytes <= pMaxBytes)
    {
        return tResult;
    }

    std::vector<const TileFiles*> tOrder;
    for (const auto& tEntry : tTiles)
    {
        tOrder.push_back(&tEntry.second);
    }
    std::sort(tOrder.begin(), tOrder.end(),
              [](const TileFiles* pFirst, const TileFiles* pSecond)
              {
                  return pFirst->mNewest < pSecond->mNewest;  // en eski once
              });
    const uint64_t tTarget = static_cast<uint64_t>(static_cast<double>(pMaxBytes) * kTrimTarget);
    uint64_t tRemaining = tResult.mTotalBytes;
    for (const TileFiles* tTile : tOrder)
    {
        if (tRemaining <= tTarget)
        {
            break;
        }
        for (const fs::path& tPath : tTile->mPaths)
        {
            fs::remove(tPath, tError);
        }
        tRemaining -= tTile->mBytes;
        tResult.mRemovedBytes += tTile->mBytes;
        ++tResult.mRemovedTiles;
    }
    return tResult;
}
