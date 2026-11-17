// Acilista tum apt.dat dosyalarini bir kez tarayip havalimani ozetleri ve dosya bayt
// araliklarini cikarir (Global Airports ~325 MB; ayrinti gerektiginde dosyada o kisim
// okunur). Onbellek <onbellek>/airport_index.bin; anahtar tutmazsa yeniden taranir.
// Dosya sirasi X-Plane kuralidir: ayni kimlik icin once gelen kazanir (ozel sahne Global
// Airports'u ezer): scenery_packs.ini'deki etkin paketler, sonra default apt dat.
#ifndef PRIME_MAP_SRC_DATA_APTDATINDEXER_H
#define PRIME_MAP_SRC_DATA_APTDATINDEXER_H

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "data/AirportGeometry.h"

// Satirlari sirayla alip havalimani ozetleri cikaran tarayici; dosyadan bagimsizdir.
class AptDatScanner
{
public:
    // pSeen: onceki dosyalardan alinmis kimlikler (bunlar atlanir)
    AptDatScanner(int pFileIndex, std::unordered_set<std::string>& pSeen, std::vector<AirportGeometry>& pOut);

    // pOffset: satirin dosyadaki ilk bayti
    void addLine(std::string_view pLine, uint64_t pOffset);
    void finish(uint64_t pFileSize);

    int airportCount() const;
    int runwayCount() const;
    int nodeCount() const;

private:
    void close(uint64_t pEndOffset);
    void extend(const LatLon& pPoint);

    int mFileIndex;
    std::unordered_set<std::string>& mSeen;
    std::vector<AirportGeometry>& mOut;

    AirportGeometry mCurrent;
    bool mOpen = false;
    bool mSkip = false;  // kimlik onceki bir dosyadan alindi
    bool mHasBounds = false;
    bool mHasDatumLat = false;
    bool mHasDatumLon = false;
    int mAirportCount = 0;
    int mRunwayCount = 0;
    int mNodeCount = 0;
};

class AptDatIndexer : public IAirportGeometrySource
{
public:
    const char* name() const override;
    void setCacheDirectory(const std::string& pDirectory) override;
    bool loadAll(const std::string& pXPlaneDirectory, const std::atomic<bool>& pCancel, AirportIndex& pOut,
                 std::string& pOutReport) override;

    static bool loadCache(const std::string& pPath, const std::string& pKey, AirportIndex& pOut);
    static bool saveCache(const std::string& pPath, const std::string& pKey, const AirportIndex& pIndex);
    static std::string cacheKey(const std::vector<std::string>& pFiles);  // yol | boyut | degisiklik zamani

    // Okunacak apt.dat yollari, oncelik sirasiyla (var olup olmadigina bakmaz)
    static std::vector<std::string> aptFiles(const std::string& pXPlaneDirectory);

private:
    std::string mCacheDirectory;

    static bool scanFile(const std::string& pPath, AptDatScanner& pScanner, const std::atomic<bool>& pCancel);
};
#endif  // PRIME_MAP_SRC_DATA_APTDATINDEXER_H
