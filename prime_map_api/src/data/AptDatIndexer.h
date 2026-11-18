// Tum apt.dat dosyalarini tarayip havalimani ozetlerini cikarir ve diskte onbellekler.
#ifndef PRIME_MAP_SRC_DATA_APTDATINDEXER_H
#define PRIME_MAP_SRC_DATA_APTDATINDEXER_H

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "data/AirportGeometry.h"

class AptDatScanner
{
public:
    AptDatScanner(int pFileIndex, std::unordered_set<std::string>& pSeen, std::vector<AirportGeometry>& pOut);

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
    bool mSkip = false;
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
    static std::string cacheKey(const std::vector<std::string>& pFiles);

    static std::vector<std::string> aptFiles(const std::string& pXPlaneDirectory);

private:
    std::string mCacheDirectory;

    static bool scanFile(const std::string& pPath, AptDatScanner& pScanner, const std::atomic<bool>& pCancel);
};
#endif
