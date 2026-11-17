// Kurulu DSF dizini: her 1x1 derece karo icin aday DSF'ler, X-Plane oncelik sirasiyla
// (Custom Scenery: scenery_packs.ini sirasi, sonra Global Scenery). Temel mesh'i olmayan
// karo icin X-Plane'in dunya tablosu kullanilir: orada olmayan karo DENIZ, olan ama
// sahnesi kurulu olmayan VERI YOK; tablo dogrulanamazsa 10x10 klasor yedegi uygulanir.
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
    int64_t mModified = 0;  // onbellek anahtari icin degisiklik zamani
};

enum class TileCoverage
{
    hasDsf,
    Ocean,   // bolge kurulu ama DSF yok: deniz
    NoData,  // bolge kurulu degil
};

class SceneryIndex
{
public:
    void build(const std::string& pXPlaneDirectory);

    // Test/Build icin: pCustomPacks once, pGlobalPacks sonra taranir; deniz tespiti yalnizca pGlobalPacks'e bakar.
    void buildFromPacks(const std::vector<std::string>& pCustomPacks, const std::vector<std::string>& pGlobalPacks);

    const std::vector<DsfCandidate>* candidatesFor(int pWest, int pSouth) const;
    TileCoverage coverageOf(int pWest, int pSouth) const;
    // Temel mesh'i olmayan karo (DSF yok ya da sadece overlay): Ocean ya da NoData
    TileCoverage noBaseCoverage(int pWest, int pSouth) const;
    // Karonun 10x10 derece klasoru bir Global Scenery paketinde var mi
    bool isRegionInstalled(int pWest, int pSouth) const;

    // Dunya tablosunu Global Scenery karolariyla dogrular; tutmazsa tablo kullanilmaz (yedek kural).
    void setWorldMap(const WorldTileMap& pXMap);
    bool usesWorldMap() const;
    const std::string& worldMapReport() const;

    int tileCount() const;  // en az bir DSF'i olan karo sayisi
    int fileCount() const;
    int packCount() const;

    static int tileKey(int pWest, int pSouth);
    static std::string tileName(int pWest, int pSouth);    // "+47-123"
    static std::string folderName(int pWest, int pSouth);  // "+40-130"
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
#endif  // PRIME_MAP_SRC_DATA_SCENERYINDEX_H
