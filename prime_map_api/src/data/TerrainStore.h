// Zemin karolarini arka planda ureten, onbellekleyen ve onceliklendiren isci.
#ifndef PRIME_MAP_SRC_DATA_TERRAINSTORE_H
#define PRIME_MAP_SRC_DATA_TERRAINSTORE_H

#include <chrono>
#include <map>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "data/SceneryIndex.h"
#include "data/TerrainClasses.h"

class ElevationGrid;
class TerrainRaster;
class TerrainVectors;
class TerrainTriangles;
class DsfReader;

struct TerrainImageKey
{
    int mWest = 0;
    int mSouth = 0;
    int mLevel = 0;
    int mChunk = 0;
    int mStyle = 0;
    int mAreas = 0;
    int mPalette = 0;

    bool operator==(const TerrainImageKey& pOther) const
    {
        return mWest == pOther.mWest && mSouth == pOther.mSouth && mLevel == pOther.mLevel &&
               mChunk == pOther.mChunk && mStyle == pOther.mStyle && mAreas == pOther.mAreas &&
               mPalette == pOther.mPalette;
    }
};

struct TerrainImageKeyHash
{
    size_t operator()(const TerrainImageKey& pKey) const
    {
        size_t tHash = static_cast<size_t>(SceneryIndex::tileKey(pKey.mWest, pKey.mSouth));
        tHash = tHash * 8 + static_cast<size_t>(pKey.mLevel);
        tHash = tHash * 32768 + static_cast<size_t>(pKey.mChunk);
        tHash = tHash * 8 + static_cast<size_t>(pKey.mStyle);
        tHash = tHash * 4 + static_cast<size_t>(pKey.mAreas & 3);
        return tHash * 31 + static_cast<size_t>(pKey.mPalette);
    }
};

struct TerrainImage
{
    TerrainImageKey mKey;
    int mSize = 0;
    std::vector<uint8_t> mRgba;
    std::vector<int> mMissingNeighbors;
};

enum class TerrainTileState
{
    Unknown,
    Ready,
    Ocean,
    NoData,
    Failed,
};

class TerrainStore
{
public:
    static const int kLevelCount = 6;
    static const int kDetailLevel = 4;
    static const int kFineLevel = 5;
    static int imageSize(int pLevel);
    static int chunksPerSide(int pLevel);

    TerrainStore();
    ~TerrainStore();

    TerrainStore(const TerrainStore&) = delete;
    TerrainStore& operator=(const TerrainStore&) = delete;

    void start(const std::string& pXPlaneDirectory, const std::string& pCacheDirectory, const TerrainPalette& pPalette,
               const std::vector<TerrainImageStyle>& pStyles = {});
    void stop();

    void setCacheLimit(uint64_t pBytes);

    void setMemoryScale(double pScale);

    int addPalette(const TerrainPalette& pPalette);

    bool isIndexReady() const;
    TileCoverage coverageOf(int pWest, int pSouth) const;
    TerrainTileState stateOf(int pWest, int pSouth) const;

    void beginFrame();
    void request(const TerrainImageKey& pKey);
    bool popImage(TerrainImage* pOut);
    bool isIdle() const;
    std::string statusText() const;
    std::string takeLogLines();
    bool hasElevation(int pWest, int pSouth) const;

    void requestVectors(int pWest, int pSouth);
    bool popVectors(std::shared_ptr<const TerrainVectors>* pOut);
    void forgetVectors(int pWest, int pSouth);

    void setFocus(double pLat, double pLon);
    void setPrefetchTiles(const std::vector<int>& pTiles);
    void startPrecache(const std::vector<int>& pTiles);
    void cancelPrecache();
    bool precacheProgress(int* pDone, int* pTotal) const;

    void setDiagnostics(bool pOn);

    static bool buildRaster(const SceneryIndex& pIndex, int pWest, int pSouth, const std::string& pCacheDirectory,
                            TerrainRaster* pOut, std::string* pLog, TerrainTileState* pState,
                            ElevationGrid* pElevation = nullptr, TerrainVectors* pVectors = nullptr);

    static bool loadVectors(const SceneryIndex& pIndex, int pWest, int pSouth, const std::string& pCacheDirectory,
                            TerrainVectors* pOut);
    static bool loadElevation(const SceneryIndex& pIndex, int pWest, int pSouth, const std::string& pCacheDirectory,
                              ElevationGrid* pOut);

    static bool loadTriangles(const SceneryIndex& pIndex, int pWest, int pSouth, TerrainTriangles* pOut,
                              std::string* pLog);

private:
    struct Pending
    {
        double mDistance = 0.0;
        uint64_t mFrame;
        std::chrono::steady_clock::time_point mFirst;
    };

    struct WorkStats
    {
        std::map<int, int> mRasterMisses;
        std::map<int, int> mTriangleLoads;
        std::map<int, int> mVectorMisses;
        int mElevationLoads = 0;
        double mRasterMissMs = 0.0;
        double mTriangleMs = 0.0;
        int mLatencyCount[6] = {};
        double mQueueMs[6] = {};
        double mReadyMs[6] = {};
        double mReadyMaxMs[6] = {};
        int mBlockedCount = 0;
        double mBlockedMs = 0.0;
        double mBackgroundMs = 0.0;
        int mBackgroundCount = 0;
    };

    void workerLoop();
    bool pickRequest(TerrainImageKey* pOut);
    TerrainRaster* rasterFor(int pWest, int pSouth);
    const ElevationGrid* elevationFor(int pWest, int pSouth, int pLevel);
    void keepElevation(int pWest, int pSouth, std::unique_ptr<ElevationGrid> pGrid);
    void trimCache();
    bool serveBackgroundTile();
    void keepCoarseElevation(int pKey, std::unique_ptr<ElevationGrid> pGrid);
    void buildImageFor(const TerrainImageKey& pKey, const TerrainRaster& pRaster, TerrainImage* pOut);
    std::shared_ptr<TerrainVectors> vectorsFor(int pWest, int pSouth);
    bool serveVectorRequest();
    void noteImageTime(int pLevel, double pMilliseconds);
    void logImageTimes();
    TerrainTriangles* trianglesFor(int pWest, int pSouth);

    static bool openBaseMesh(const std::vector<DsfCandidate>& pCandidates, const std::string& pName,
                             std::vector<uint8_t>* pData, DsfReader* pReader, std::string* pPath,
                             std::string* pLog);
    void setState(int pWest, int pSouth, TerrainTileState pState);
    void addLog(const std::string& pLine);

    std::string mXPlaneDirectory;
    std::string mCacheDirectory;
    std::atomic<uint64_t> mCacheLimit{0};
    std::atomic<bool> mTrimWanted{false};
    bool mCacheGrew = false;
    std::vector<TerrainPalette> mPalettes;
    std::atomic<double> mMemoryScale{1.0};
    size_t scaledLimit(size_t pBase) const;
    std::vector<TerrainImageStyle> mStyles;
    SceneryIndex mIndex;
    std::atomic<bool> mIndexReady{false};

    std::thread mThread;
    mutable std::mutex mMutex;
    std::condition_variable mWake;
    bool mStopping = false;
    uint64_t mFrame = 0;
    std::unordered_map<TerrainImageKey, Pending, TerrainImageKeyHash> mRequests;
    std::deque<TerrainImage> mResults;
    std::unordered_map<int, TerrainTileState> mStates;
    std::string mLog;
    std::string mStatus;

    double mFocusLat = 0.0;
    double mFocusLon = 0.0;
    std::deque<int> mPrefetch;
    std::deque<int> mPrecache;
    int mPrecacheDone = 0;
    int mPrecacheTotal = 0;
    std::unordered_set<int> mBackgroundDone;

    bool mHasInFlight = false;
    TerrainImageKey mInFlight;
    std::unordered_set<int> mElevationReady;
    std::unordered_map<int, uint64_t> mVectorRequests;
    std::unordered_set<int> mVectorsServed;
    std::deque<std::shared_ptr<const TerrainVectors>> mVectorResults;

    std::list<std::pair<int, std::unique_ptr<TerrainRaster>>> mRasters;

    std::list<std::pair<int, std::unique_ptr<ElevationGrid>>> mElevations;
    std::list<std::pair<int, std::unique_ptr<ElevationGrid>>> mCoarseElevations;

    std::list<std::pair<int, std::shared_ptr<TerrainVectors>>> mVectors;

    WorkStats mStats;
    std::atomic<bool> mDiagnostics{false};
    std::chrono::steady_clock::time_point mPickedFirst;
    int mImageCounts[kLevelCount] = {};
    double mImageMilliseconds[kLevelCount] = {};

    std::list<std::pair<int, std::unique_ptr<TerrainTriangles>>> mTriangles;
};
#endif
