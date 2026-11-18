// Havalimani ozetlerinin deposu ve mekansal sorgusu.
#ifndef PRIME_MAP_SRC_DATA_AIRPORTGEOMETRYSTORE_H
#define PRIME_MAP_SRC_DATA_AIRPORTGEOMETRYSTORE_H

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "GeoGrid.h"
#include "data/AirportGeometry.h"

class AirportGeometryStore
{
public:
    explicit AirportGeometryStore(std::unique_ptr<IAirportGeometrySource> pSource);
    ~AirportGeometryStore();

    AirportGeometryStore(const AirportGeometryStore&) = delete;
    AirportGeometryStore& operator=(const AirportGeometryStore&) = delete;

    void startLoading(const std::string& pXPlaneDirectory, const std::string& pCacheDirectory = "");

    bool poll();

    bool isLoading() const;
    bool isReady() const;
    bool lastLoadSucceeded() const;
    const std::string& lastReport() const;
    const char* sourceName() const;

    int count() const;
    int runwayCount() const;
    const AirportGeometry& at(int pIndex) const;
    const AirportGeometry* find(const std::string& pId) const;
    const std::string& filePath(int pFile) const;

    void query(const GeoBounds& pBounds, std::vector<int>& pOut) const;

private:
    void takeResult();

    std::unique_ptr<IAirportGeometrySource> mSource;
    std::thread mThread;
    std::atomic<bool> mCancel{false};
    std::atomic<bool> mFinished{false};

    AirportIndex mPending;
    std::string mPendingReport;
    bool mPendingSucceeded = false;

    std::vector<AirportGeometry> mAirports;
    std::vector<std::string> mFiles;
    std::unordered_map<std::string, int> mById;
    GeoGrid mGrid;
    std::string mReport;
    bool mLoading = false;
    bool mReady = false;
    bool mSucceeded = false;
    int mRunwayCount = 0;
};
#endif
