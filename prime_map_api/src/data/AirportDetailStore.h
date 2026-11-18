// Ekrandaki havalimanlarinin ayrintisini arka planda okuyup cizime hazirlayan depo.
#ifndef PRIME_MAP_SRC_DATA_AIRPORTDETAILSTORE_H
#define PRIME_MAP_SRC_DATA_AIRPORTDETAILSTORE_H

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "data/AirportDetail.h"
#include "data/AirportShapes.h"

class AirportGeometryStore;

struct LoadedAirport
{
    AirportDetail mDetail;
    AirportShapes mShapes;
    double mLoadMs = 0.0;
};

class AirportDetailStore
{
public:
    static const size_t kMaxLoaded = 64;

    explicit AirportDetailStore(const AirportGeometryStore& pSummaries);
    ~AirportDetailStore();

    AirportDetailStore(const AirportDetailStore&) = delete;
    AirportDetailStore& operator=(const AirportDetailStore&) = delete;

    void setMaxLoaded(size_t pCount);

    void start();
    void stop();

    void update();
    const LoadedAirport* use(int pAirport);

    bool isWaiting(int pAirport) const;
    int pendingCount() const;
    int loadedCount() const;
    std::string takeLogLines();

private:
    struct Request
    {
        int mAirport = -1;
        std::string mPath;
        uint64_t mOffset = 0;
        uint32_t mLength = 0;
        LatLon mReference;
        std::string mId;
    };

    struct Result
    {
        int mAirport = -1;
        std::unique_ptr<LoadedAirport> mLoaded;
    };

    struct Entry
    {
        std::unique_ptr<LoadedAirport> mLoaded;
        uint64_t mLastFrame = 0;
    };

    void submitWanted();
    void workerLoop();
    std::unique_ptr<LoadedAirport> load(const Request& pRequest, std::string& pLog) const;
    void evictUnused();

    const AirportGeometryStore& mSummaries;

    std::unordered_map<int, Entry> mLoaded;
    std::unordered_set<int> mFailed;
    std::vector<int> mWanted;
    std::unordered_set<int> mWantedSet;
    uint64_t mFrame = 0;
    int mPendingCount = 0;
    size_t mMaxLoaded = kMaxLoaded;

    mutable std::mutex mMutex;
    std::condition_variable mWake;
    std::deque<Request> mQueue;
    std::unordered_set<int> mInFlight;
    std::vector<Result> mResults;
    std::string mLog;
    bool mStopping = false;
    std::thread mThread;
};
#endif
