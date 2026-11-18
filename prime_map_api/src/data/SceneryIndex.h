// Kurulu sahnenin DSF dizini: her karo icin X-Plane oncelik sirasina gore aday dosyalar.
#ifndef PRIME_MAP_SRC_DATA_SCENERYINDEX_H
#define PRIME_MAP_SRC_DATA_SCENERYINDEX_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "data/WorldTileMap.h"

struct DsfCandidate
{
    std::string mPath;
    uint64_t mSize = 0;
    int64_t mModified = 0;
};

enum class TileCoverage
{
    hasDsf,
    Ocean,
    NoData,
};

class SceneryIndex
{
public:
    void build(const std::string& pXPlaneDirectory);

    void buildFromPacks(const std::vector<std::string>& pCustomPacks, const std::vector<std::string>& pGlobalPacks);

    const std::vector<DsfCandidate>* candidatesFor(int pWest, int pSouth) const;
    TileCoverage coverageOf(int pWest, int pSouth) const;
    TileCoverage noBaseCoverage(int pWest, int pSouth) const;
    bool isRegionInstalled(int pWest, int pSouth) const;

    void setWorldMap(const WorldTileMap& pXMap);
    bool usesWorldMap() const;
    const std::string& worldMapReport() const;

    int tileCount() const;
    int fileCount() const;
    int packCount() const;

    static int tileKey(int pWest, int pSouth);
    static std::string tileName(int pWest, int pSouth);
    static std::string folderName(int pWest, int pSouth);
    static bool parseTileName(const std::string& pFileName, int* pOutWest, int* pOutSouth);

private:
    void scanPack(const std::string& pPackDirectory, bool pGlobal);

    std::unordered_map<int, std::vector<DsfCandidate>> mTiles;
    std::unordered_set<int> mInstalledFolders;
    std::unordered_set<int> mGlobalTiles;
    WorldTileMap mWorldMap;
    std::string mWorldMapReport;
    int mFileCount = 0;
    int mPackCount = 0;
};
#endif
