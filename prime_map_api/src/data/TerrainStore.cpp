#include "data/TerrainStore.h"
#include "data/TerrainProfile.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <optional>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

#include "data/CacheTrim.h"
#include "data/AtomicFile.h"
#include "data/DsfReader.h"
#include "data/SevenZip.h"
#include "data/TerrainElevation.h"
#include "data/TerrainRaster.h"
#include "data/TerrainTriangles.h"
#include "data/TerrainVectors.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace
{
const size_t kMaxRastersInMemory = 3;
const size_t kMaxTriangleTiles = 2;
const size_t kMaxElevationTiles = 12;
const size_t kMaxCoarseElevationTiles = 64;
const int kCoarseElevationLevel = 2;
const size_t kMaxVectorTiles = 4;
const uint64_t kStaleFrames = 120;
const size_t kMaxQueuedResults = 64;
const char* kCacheVersion = "terrain-v1";

using Clock = std::chrono::steady_clock;

double millisecondsSince(Clock::time_point pStart)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - pStart).count();
}

int wrapWest(int pWest)
{
    return pWest < -180 ? pWest + 360 : (pWest > 179 ? pWest - 360 : pWest);
}

std::string cacheKey(const std::vector<DsfCandidate>& pCandidates)
{
    std::string tKey = kCacheVersion;
    for (const DsfCandidate& tCandidate : pCandidates)
    {
        tKey += "|" + tCandidate.mPath + "|" + std::to_string(tCandidate.mSize) + "|" +
                std::to_string(tCandidate.mModified);
    }
    return tKey;
}

bool hasNoBaseMarker(const std::string& pPath, const std::string& pKey)
{
    std::ifstream tFile(pPath, std::ios::binary);
    if (!tFile)
    {
        return false;
    }
    const std::string tKey((std::istreambuf_iterator<char>(tFile)), std::istreambuf_iterator<char>());
    return tKey == pKey;
}

void writeNoBaseMarker(const std::string& pPath, const std::string& pKey)
{
    writeFileAtomic(pPath,
                    [&pKey](std::ofstream& tFile)
                    {
                        tFile << pKey;
                    });
}

double focusDistance(const TerrainImageKey& pKey, double pFocusLat, double pFocusLon);

bool readWholeFile(const std::string& pPath, std::vector<uint8_t>* pOut)
{
    std::ifstream tFile(pPath, std::ios::binary);
    if (!tFile)
    {
        return false;
    }
    pOut->assign(std::istreambuf_iterator<char>(tFile), std::istreambuf_iterator<char>());
    return true;
}
}

int TerrainStore::imageSize(int pLevel)
{
    const int kSizes[kLevelCount] = {64, 256, 1024, 1024, 1024, 1024};
    return kSizes[pLevel];
}

int TerrainStore::chunksPerSide(int pLevel)
{
    if (pLevel == kDetailLevel)
    {
        return TerrainTriangles::kChunksPerSide;
    }
    if (pLevel == kFineLevel)
    {
        return TerrainTriangles::kChunksPerSide * 4;
    }
    return pLevel == 3 ? 4 : 1;
}

TerrainStore::TerrainStore() = default;

TerrainStore::~TerrainStore()
{
    stop();
}

void TerrainStore::start(const std::string& pXPlaneDirectory, const std::string& pCacheDirectory,
                         const TerrainPalette& pPalette, const std::vector<TerrainImageStyle>& pStyles)
{
    if (mThread.joinable())
    {
        return;
    }
    mXPlaneDirectory = pXPlaneDirectory;
    mCacheDirectory = pCacheDirectory;
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        if (mPalettes.empty())
        {
            mPalettes.push_back(pPalette);
        }
        else
        {
            mPalettes[0] = pPalette;
        }
    }
    mStyles = pStyles;
    mStopping = false;
    mThread = std::thread(
        [this]()
        {
            workerLoop();
        });
#if defined(_WIN32)
    SetThreadPriority(static_cast<HANDLE>(mThread.native_handle()), THREAD_PRIORITY_BELOW_NORMAL);
#endif
}

void TerrainStore::setCacheLimit(uint64_t pBytes)
{
    if (mCacheLimit.exchange(pBytes) != pBytes)
    {
        mTrimWanted.store(true);
        mWake.notify_one();
    }
}

void TerrainStore::setMemoryScale(double pScale)
{
    mMemoryScale.store(std::max(1.0, pScale));
}

size_t TerrainStore::scaledLimit(size_t pBase) const
{
    return static_cast<size_t>(std::lround(static_cast<double>(pBase) * mMemoryScale.load()));
}

int TerrainStore::addPalette(const TerrainPalette& pPalette)
{
    std::lock_guard<std::mutex> tLock(mMutex);
    mPalettes.push_back(pPalette);
    return static_cast<int>(mPalettes.size()) - 1;
}

void TerrainStore::trimCache()
{
    mCacheGrew = false;
    const uint64_t tLimit = mCacheLimit.load();
    if (mCacheDirectory.empty() || tLimit == 0)
    {
        return;
    }
    const CacheTrimResult tResult = trimTileCache(mCacheDirectory, tLimit);
    if (tResult.mRemovedTiles > 0)
    {
        addLog(std::format("terrain cache: {:.0f} MB over the {:.0f} MB limit, removed {} least recently used tiles "
                           "({:.0f} MB)",
                           tResult.mTotalBytes / 1048576.0, tLimit / 1048576.0, tResult.mRemovedTiles,
                           tResult.mRemovedBytes / 1048576.0));
    }
    if (tResult.mRemovedTempFiles > 0)
    {
        addLog(std::format("terrain cache: removed {} stale temporary file(s) from an earlier crash",
                           tResult.mRemovedTempFiles));
    }
}

void TerrainStore::setFocus(double pLat, double pLon)
{
    std::lock_guard<std::mutex> tLock(mMutex);
    mFocusLat = pLat;
    mFocusLon = pLon;
}

void TerrainStore::setPrefetchTiles(const std::vector<int>& pTiles)
{
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        mPrefetch.assign(pTiles.begin(), pTiles.end());
    }
    mWake.notify_one();
}

void TerrainStore::startPrecache(const std::vector<int>& pTiles)
{
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        mPrecache.assign(pTiles.begin(), pTiles.end());
        mPrecacheDone = 0;
        mPrecacheTotal = static_cast<int>(pTiles.size());
    }
    mWake.notify_one();
}

void TerrainStore::cancelPrecache()
{
    std::lock_guard<std::mutex> tLock(mMutex);
    mPrecache.clear();
    mPrecacheTotal = mPrecacheDone;
}

bool TerrainStore::precacheProgress(int* pDone, int* pTotal) const
{
    std::lock_guard<std::mutex> tLock(mMutex);
    *pDone = mPrecacheDone;
    *pTotal = mPrecacheTotal;
    return mPrecacheDone < mPrecacheTotal;
}

bool TerrainStore::serveBackgroundTile()
{
    int tKey = -1;
    bool tPrecache = false;
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        if (!mRequests.empty() || !mVectorRequests.empty())
        {
            return false;
        }
        while (tKey < 0 && (!mPrecache.empty() || !mPrefetch.empty()))
        {
            tPrecache = !mPrecache.empty();
            std::deque<int>& tQueue = tPrecache ? mPrecache : mPrefetch;
            const int tCandidate = tQueue.front();
            tQueue.pop_front();
            if (mBackgroundDone.count(tCandidate) == 0)
            {
                tKey = tCandidate;
            }
            else if (tPrecache)
            {
                ++mPrecacheDone;
            }
        }
        if (tKey < 0)
        {
            return false;
        }
    }
    const int tWest = tKey % 360 - 180;
    const int tSouth = tKey / 360 - 90;
    bool tInMemory = false;
    for (const auto& tEntry : mRasters)
    {
        tInMemory = tInMemory || tEntry.first == tKey;
    }
    if (!tInMemory)
    {
        TerrainRaster tRaster;
        ElevationGrid tElevation;
        TerrainVectors tVectors;
        std::string tLog;
        TerrainTileState tState = TerrainTileState::Unknown;
        const Clock::time_point tBackgroundStart = Clock::now();
        buildRaster(mIndex, tWest, tSouth, mCacheDirectory, &tRaster, &tLog, &tState, &tElevation, &tVectors);
        const double tBackgroundMs = millisecondsSince(tBackgroundStart);
        mStats.mBackgroundMs += tBackgroundMs;
        ++mStats.mBackgroundCount;
        {
            std::lock_guard<std::mutex> tLock(mMutex);
            if (!mRequests.empty())
            {
                ++mStats.mBlockedCount;
                mStats.mBlockedMs += tBackgroundMs;
            }
        }
        if (!tLog.empty())
        {
            tLog.pop_back();
            addLog((tPrecache ? "pre-cache: " : "prefetch: ") + tLog);
        }
        setState(tWest, tSouth, tState);
        mCacheGrew = true;
    }
    std::lock_guard<std::mutex> tLock(mMutex);
    mBackgroundDone.insert(tKey);
    if (tPrecache)
    {
        ++mPrecacheDone;
    }
    return true;
}

void TerrainStore::stop()
{
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        mStopping = true;
    }
    mWake.notify_all();
    if (mThread.joinable())
    {
        mThread.join();
    }
}

bool TerrainStore::isIndexReady() const
{
    return mIndexReady.load();
}

TileCoverage TerrainStore::coverageOf(int pWest, int pSouth) const
{
    if (!mIndexReady.load())
    {
        return TileCoverage::NoData;
    }
    return mIndex.coverageOf(pWest, pSouth);
}

TerrainTileState TerrainStore::stateOf(int pWest, int pSouth) const
{
    std::lock_guard<std::mutex> tLock(mMutex);
    const auto tFound = mStates.find(SceneryIndex::tileKey(pWest, pSouth));
    return tFound == mStates.end() ? TerrainTileState::Unknown : tFound->second;
}

void TerrainStore::beginFrame()
{
    std::lock_guard<std::mutex> tLock(mMutex);
    ++mFrame;
}

void TerrainStore::request(const TerrainImageKey& pKey)
{
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        if (mHasInFlight && mInFlight == pKey)
        {
            return;
        }
        for (const TerrainImage& tResult : mResults)
        {
            if (tResult.mKey == pKey)
            {
                return;
            }
        }
        const auto [tEntry, tInserted] = mRequests.try_emplace(pKey);
        tEntry->second.mFrame = mFrame;
        tEntry->second.mDistance = focusDistance(pKey, mFocusLat, mFocusLon);
        if (tInserted)
        {
            tEntry->second.mFirst = Clock::now();
        }
    }
    mWake.notify_one();
}

void TerrainStore::setDiagnostics(bool pOn)
{
    mDiagnostics.store(pOn);
}

bool TerrainStore::hasElevation(int pWest, int pSouth) const
{
    std::lock_guard<std::mutex> tLock(mMutex);
    return mElevationReady.count(SceneryIndex::tileKey(pWest, pSouth)) > 0;
}

bool TerrainStore::popImage(TerrainImage* pOut)
{
    std::lock_guard<std::mutex> tLock(mMutex);
    if (mResults.empty())
    {
        return false;
    }
    *pOut = std::move(mResults.front());
    mResults.pop_front();
    mWake.notify_one();
    return true;
}

bool TerrainStore::isIdle() const
{
    std::lock_guard<std::mutex> tLock(mMutex);
    return mRequests.empty() && mResults.empty() && mVectorRequests.empty() && mVectorResults.empty() &&
           !mHasInFlight && mStatus.empty() && mIndexReady.load();
}

void TerrainStore::requestVectors(int pWest, int pSouth)
{
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        const int tKey = SceneryIndex::tileKey(pWest, pSouth);
        if (mVectorsServed.count(tKey))
        {
            return;
        }
        mVectorRequests[tKey] = mFrame;
    }
    mWake.notify_one();
}

bool TerrainStore::popVectors(std::shared_ptr<const TerrainVectors>* pOut)
{
    std::lock_guard<std::mutex> tLock(mMutex);
    if (mVectorResults.empty())
    {
        return false;
    }
    *pOut = std::move(mVectorResults.front());
    mVectorResults.pop_front();
    return true;
}

std::string TerrainStore::statusText() const
{
    std::lock_guard<std::mutex> tLock(mMutex);
    return mStatus;
}

std::string TerrainStore::takeLogLines()
{
    std::lock_guard<std::mutex> tLock(mMutex);
    std::string tLines;
    tLines.swap(mLog);
    return tLines;
}

void TerrainStore::addLog(const std::string& pLine)
{
    std::lock_guard<std::mutex> tLock(mMutex);
    mLog += pLine + "\n";
}

void TerrainStore::setState(int pWest, int pSouth, TerrainTileState pState)
{
    std::lock_guard<std::mutex> tLock(mMutex);
    mStates[SceneryIndex::tileKey(pWest, pSouth)] = pState;
}

namespace
{
double focusDistance(const TerrainImageKey& pKey, double pFocusLat, double pFocusLon)
{
    const int tCount = TerrainStore::chunksPerSide(pKey.mLevel);
    const int tColumn = tCount > 1 ? pKey.mChunk % tCount : 0;
    const int tRow = tCount > 1 ? pKey.mChunk / tCount : 0;
    const double tLon = pKey.mWest + (tColumn + 0.5) / tCount;
    const double tLat = pKey.mSouth + 1.0 - (tRow + 0.5) / tCount;
    double tDeltaLon = std::abs(tLon - pFocusLon);
    tDeltaLon = std::min(tDeltaLon, 360.0 - tDeltaLon);
    const double tCos = std::cos(pFocusLat * 3.14159265358979 / 180.0);
    const double tDeltaLat = tLat - pFocusLat;
    return tDeltaLon * tDeltaLon * tCos * tCos + tDeltaLat * tDeltaLat;
}
}

bool TerrainStore::pickRequest(TerrainImageKey* pOut)
{
    bool tFound = false;
    uint64_t tBestFrame = 0;
    int tBestLevel = 0;
    double tBestDistance = 0.0;
    for (auto tIterator = mRequests.begin(); tIterator != mRequests.end();)
    {
        if (tIterator->second.mFrame + kStaleFrames < mFrame)
        {
            tIterator = mRequests.erase(tIterator);
            continue;
        }
        const uint64_t tFrame = tIterator->second.mFrame;
        const int tLevel = tIterator->first.mLevel;
        const double tDistance = tIterator->second.mDistance;
        const bool tBetter = !tFound || tFrame > tBestFrame ||
                             (tFrame == tBestFrame && (tLevel < tBestLevel ||
                                                       (tLevel == tBestLevel && tDistance < tBestDistance)));
        if (tBetter)
        {
            tFound = true;
            tBestFrame = tFrame;
            tBestLevel = tLevel;
            tBestDistance = tDistance;
            *pOut = tIterator->first;
        }
        ++tIterator;
    }
    if (tFound)
    {
        mPickedFirst = mRequests[*pOut].mFirst;
        mRequests.erase(*pOut);
    }
    return tFound;
}

bool TerrainStore::buildRaster(const SceneryIndex& pIndex, int pWest, int pSouth, const std::string& pCacheDirectory,
                               TerrainRaster* pOut, std::string* pLog, TerrainTileState* pState,
                               ElevationGrid* pElevation, TerrainVectors* pVectors)
{
    const TerrainTileState tNoBaseState = pIndex.noBaseCoverage(pWest, pSouth) == TileCoverage::Ocean
                                              ? TerrainTileState::Ocean
                                              : TerrainTileState::NoData;
    const std::vector<DsfCandidate>* tCandidates = pIndex.candidatesFor(pWest, pSouth);
    if (!tCandidates)
    {
        *pState = tNoBaseState;
        return false;
    }
    const std::string tName = SceneryIndex::tileName(pWest, pSouth);
    const std::string tCachePath = pCacheDirectory + tName + ".terrain";
    const std::string tElevationPath = pCacheDirectory + tName + ".elevation";
    const std::string tVectorsPath = pCacheDirectory + tName + ".vectors";
    const std::string tNoBasePath = pCacheDirectory + tName + ".nobase";
    const std::string tKey = cacheKey(*tCandidates);
    auto tLoadRaster = [&]()
    {
        TerrainProfile::ScopedStage tStage(TerrainProfile::Stage::loadRaster);
        return pOut->load(tCachePath, tKey);
    };
    auto tLoadElevation = [&]()
    {
        TerrainProfile::ScopedStage tStage(TerrainProfile::Stage::loadElevation);
        return !pElevation || pElevation->load(tElevationPath, tKey);
    };
    auto tLoadVectors = [&]()
    {
        TerrainProfile::ScopedStage tStage(TerrainProfile::Stage::loadVectors);
        return !pVectors || pVectors->load(tVectorsPath, tKey);
    };
    if (!pCacheDirectory.empty() && tLoadRaster() && tLoadElevation() && tLoadVectors())
    {
        *pState = TerrainTileState::Ready;
        touchCacheFile(tCachePath);
        return true;
    }
    if (!pCacheDirectory.empty() && hasNoBaseMarker(tNoBasePath, tKey))
    {
        *pState = tNoBaseState;
        return false;
    }

    const Clock::time_point tStart = Clock::now();
    std::vector<uint8_t> tData;
    DsfReader tReader;
    std::string tPath;
    if (!openBaseMesh(*tCandidates, tName, &tData, &tReader, &tPath, pLog))
    {
        *pState = tNoBaseState;
        if (!pCacheDirectory.empty())
        {
            writeNoBaseMarker(tNoBasePath, tKey);
        }
        *pLog += "terrain " + tName + ": no base mesh (only overlays)\n";
        return false;
    }

    std::vector<TerrainClass> tClasses;
    for (const std::string& tTerrain : tReader.info().mTerrainNames)
    {
        tClasses.push_back(classifyTerrain(tTerrain));
    }
    pOut->reset(pWest, pSouth);
    std::string tError;
    std::optional<TerrainProfile::ScopedStage> tTriangleStage(TerrainProfile::Stage::DsfTriangles);
    const bool tOk = tReader.forEachTriangle(
        [&](const DsfTriangle& pTriangle)
        {
            if ((pTriangle.mPatchFlags & DsfReader::kPatchFlagOverlay) == 0 && pTriangle.mTerrain >= 0 &&
                pTriangle.mTerrain < static_cast<int>(tClasses.size()))
            {
                pOut->fillTriangle(pTriangle.mLon, pTriangle.mLat, tClasses[pTriangle.mTerrain]);
            }
        },
        &tError);
    if (!tOk)
    {
        *pLog += "terrain " + tName + ": " + tError + " (" + tPath + ")\n";
        *pState = TerrainTileState::Failed;
        return false;
    }
    tTriangleStage.reset();
    std::optional<TerrainProfile::ScopedStage> tVectorStage(TerrainProfile::Stage::DsfVectors);
    TerrainVectors tLocalVectors;
    TerrainVectors& tVectors = pVectors ? *pVectors : tLocalVectors;
    tVectors.reset(pWest, pSouth);
    std::string tVectorError;
    if (!tVectors.addFromDsf(tReader, &tVectorError))
    {
        *pLog += "terrain " + tName + ": vectors skipped: " + tVectorError + "\n";
        tVectors.reset(pWest, pSouth);
    }
    tVectorStage.emplace(TerrainProfile::Stage::Overlays);
    for (const DsfCandidate& tCandidate : *tCandidates)
    {
        if (tCandidate.mPath == tPath)
        {
            continue;
        }
        std::vector<uint8_t> tOverlayData;
        DsfReader tOverlay;
        std::string tOverlayError;
        if (!readWholeFile(tCandidate.mPath, &tOverlayData))
        {
            continue;
        }
        if (SevenZip::isArchive(tOverlayData.data(), tOverlayData.size()))
        {
            std::vector<uint8_t> tPlain;
            if (!SevenZip::extractFirstFile(tOverlayData, &tPlain, &tOverlayError))
            {
                continue;
            }
            tOverlayData.swap(tPlain);
        }
        if (tOverlay.open(tOverlayData, &tOverlayError) && tOverlay.info().mOverlay)
        {
            tVectors.addExclusions(tOverlay.info());
        }
    }
    tVectors.applyNetworkExclusions();
    tVectorStage.emplace(TerrainProfile::Stage::FillAreas);
    pOut->fillAreas(tVectors);
    tVectorStage.emplace(TerrainProfile::Stage::CacheSave);
    if (!pCacheDirectory.empty() && !tVectors.save(tVectorsPath, tKey))
    {
        *pLog += "terrain " + tName + ": cannot write cache " + tVectorsPath + "\n";
    }

    if (!pCacheDirectory.empty() && !pOut->save(tCachePath, tKey))
    {
        *pLog += "terrain " + tName + ": cannot write cache " + tCachePath + "\n";
    }

    tVectorStage.reset();
    std::string tElevationText = "no elevation";
    if (pElevation)
    {
        DsfRaster tDem;
        DsfRaster tSeaLevel;
        bool tHasSea = false;
        {
            TerrainProfile::ScopedStage tStage(TerrainProfile::Stage::ElevationBuild);
            const bool tHasDem = tReader.readRaster("elevation", &tDem);
            tHasSea = tReader.readRaster("sea_level", &tSeaLevel);
            if (!tHasDem || !pElevation->build(tDem, tHasSea ? &tSeaLevel : nullptr))
            {
                *pElevation = ElevationGrid();
            }
        }
        const Clock::time_point tPackStart = Clock::now();
        {
            TerrainProfile::ScopedStage tStage(TerrainProfile::Stage::ElevationPack);
            if (!pCacheDirectory.empty() && !pElevation->save(tElevationPath, tKey))
            {
                *pLog += "terrain " + tName + ": cannot write cache " + tElevationPath + "\n";
            }
        }
        if (!pElevation->isEmpty())
        {
            std::error_code tSizeError;
            const uintmax_t tBytes = std::filesystem::file_size(tElevationPath, tSizeError);
            tElevationText = std::format("elevation {} posts, sea_level {}, packed {:.2f} MB in {:.0f} ms",
                                         pElevation->size(0), tHasSea ? "yes" : "no",
                                         tSizeError ? 0.0 : tBytes / 1048576.0, millisecondsSince(tPackStart));
        }
    }
    *pLog += std::format("terrain {}: built from {} in {:.1f} s (water {:.0f}%, {}, vectors {} areas {} lines)\n",
                         tName, tPath, std::chrono::duration<double>(Clock::now() - tStart).count(),
                         100.0 * pOut->countOf(TerrainClass::Water) / (TerrainRaster::kSize * TerrainRaster::kSize),
                         tElevationText, tVectors.areas().size(), tVectors.lines().size());
    *pState = TerrainTileState::Ready;
    return true;
}

bool TerrainStore::loadElevation(const SceneryIndex& pIndex, int pWest, int pSouth,
                                 const std::string& pCacheDirectory, ElevationGrid* pOut)
{
    const std::vector<DsfCandidate>* tCandidates = pIndex.candidatesFor(pWest, pSouth);
    if (!tCandidates || pCacheDirectory.empty())
    {
        return false;
    }
    const std::string tPath = pCacheDirectory + SceneryIndex::tileName(pWest, pSouth) + ".elevation";
    return pOut->load(tPath, cacheKey(*tCandidates));
}

bool TerrainStore::openBaseMesh(const std::vector<DsfCandidate>& pCandidates, const std::string& pName,
                                std::vector<uint8_t>* pData, DsfReader* pReader, std::string* pPath,
                                std::string* pLog)
{
    TerrainProfile::ScopedStage tStage(TerrainProfile::Stage::DsfUnpack);
    for (const DsfCandidate& tCandidate : pCandidates)
    {
        if (!readWholeFile(tCandidate.mPath, pData))
        {
            *pLog += "terrain " + pName + ": cannot read " + tCandidate.mPath + "\n";
            continue;
        }
        std::string tError;
        if (SevenZip::isArchive(pData->data(), pData->size()))
        {
            std::vector<uint8_t> tPlain;
            if (!SevenZip::extractFirstFile(*pData, &tPlain, &tError))
            {
                *pLog += "terrain " + pName + ": " + tError + " (" + tCandidate.mPath + ")\n";
                continue;
            }
            pData->swap(tPlain);
        }
        if (!pReader->open(*pData, &tError))
        {
            *pLog += "terrain " + pName + ": " + tError + " (" + tCandidate.mPath + ")\n";
            continue;
        }
        if (pReader->info().mOverlay)
        {
            continue;
        }
        *pPath = tCandidate.mPath;
        return true;
    }
    return false;
}

bool TerrainStore::loadTriangles(const SceneryIndex& pIndex, int pWest, int pSouth, TerrainTriangles* pOut,
                                 std::string* pLog)
{
    const std::vector<DsfCandidate>* tCandidates = pIndex.candidatesFor(pWest, pSouth);
    if (!tCandidates)
    {
        return false;
    }
    const std::string tName = SceneryIndex::tileName(pWest, pSouth);
    const Clock::time_point tStart = Clock::now();
    std::vector<uint8_t> tData;
    DsfReader tReader;
    std::string tPath;
    if (!openBaseMesh(*tCandidates, tName, &tData, &tReader, &tPath, pLog))
    {
        return false;
    }
    std::vector<TerrainClass> tClasses;
    for (const std::string& tTerrain : tReader.info().mTerrainNames)
    {
        tClasses.push_back(classifyTerrain(tTerrain));
    }
    pOut->reset(pWest, pSouth);
    std::string tError;
    const bool tOk = tReader.forEachTriangle(
        [&](const DsfTriangle& pTriangle)
        {
            if ((pTriangle.mPatchFlags & DsfReader::kPatchFlagOverlay) == 0 && pTriangle.mTerrain >= 0 &&
                pTriangle.mTerrain < static_cast<int>(tClasses.size()))
            {
                pOut->add(pTriangle.mLon, pTriangle.mLat, tClasses[pTriangle.mTerrain]);
            }
        },
        &tError);
    if (!tOk)
    {
        *pLog += "terrain detail " + tName + ": " + tError + " (" + tPath + ")\n";
        return false;
    }
    *pLog += std::format("terrain detail {}: {} triangles, {:.0f} MB in {:.1f} s\n", tName, pOut->triangleCount(),
                         pOut->memoryBytes() / 1048576.0,
                         std::chrono::duration<double>(Clock::now() - tStart).count());
    return true;
}

TerrainTriangles* TerrainStore::trianglesFor(int pWest, int pSouth)
{
    const int tKey = SceneryIndex::tileKey(pWest, pSouth);
    for (auto tIterator = mTriangles.begin(); tIterator != mTriangles.end(); ++tIterator)
    {
        if (tIterator->first == tKey)
        {
            mTriangles.splice(mTriangles.begin(), mTriangles, tIterator);
            return mTriangles.front().second.get();
        }
    }
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        mStatus = "Loading terrain detail " + SceneryIndex::tileName(pWest, pSouth);
    }
    std::unique_ptr<TerrainTriangles> tTriangles = std::make_unique<TerrainTriangles>();
    std::string tLog;
    const Clock::time_point tTriangleStart = Clock::now();
    const bool tOk = loadTriangles(mIndex, pWest, pSouth, tTriangles.get(), &tLog);
    ++mStats.mTriangleLoads[tKey];
    mStats.mTriangleMs += millisecondsSince(tTriangleStart);
    if (!tLog.empty())
    {
        tLog.pop_back();
        addLog(tLog);
    }
    if (!tOk)
    {
        return nullptr;
    }
    mTriangles.emplace_front(tKey, std::move(tTriangles));
    while (mTriangles.size() > scaledLimit(kMaxTriangleTiles))
    {
        mTriangles.pop_back();
    }
    return mTriangles.front().second.get();
}

TerrainRaster* TerrainStore::rasterFor(int pWest, int pSouth)
{
    const int tKey = SceneryIndex::tileKey(pWest, pSouth);
    for (auto tIterator = mRasters.begin(); tIterator != mRasters.end(); ++tIterator)
    {
        if (tIterator->first == tKey)
        {
            mRasters.splice(mRasters.begin(), mRasters, tIterator);
            return mRasters.front().second.get();
        }
    }
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        mStatus = "Building terrain " + SceneryIndex::tileName(pWest, pSouth);
    }
    std::unique_ptr<TerrainRaster> tRaster = std::make_unique<TerrainRaster>();
    std::unique_ptr<ElevationGrid> tElevation = std::make_unique<ElevationGrid>();
    std::shared_ptr<TerrainVectors> tVectors = std::make_shared<TerrainVectors>();
    std::string tLog;
    TerrainTileState tState = TerrainTileState::Unknown;
    const Clock::time_point tMissStart = Clock::now();
    const bool tOk = buildRaster(mIndex, pWest, pSouth, mCacheDirectory, tRaster.get(), &tLog, &tState,
                                 tElevation.get(), tVectors.get());
    ++mStats.mRasterMisses[tKey];
    mStats.mRasterMissMs += millisecondsSince(tMissStart);
    if (!tLog.empty())
    {
        tLog.pop_back();
        addLog(tLog);
    }
    setState(pWest, pSouth, tState);
    if (!tOk)
    {
        return nullptr;
    }
    mCacheGrew = true;
    keepElevation(pWest, pSouth, std::move(tElevation));
    mVectors.emplace_front(tKey, std::move(tVectors));
    while (mVectors.size() > scaledLimit(kMaxVectorTiles))
    {
        mVectors.pop_back();
    }
    mRasters.emplace_front(tKey, std::move(tRaster));
    while (mRasters.size() > scaledLimit(kMaxRastersInMemory))
    {
        mRasters.pop_back();
    }
    return mRasters.front().second.get();
}

void TerrainStore::keepElevation(int pWest, int pSouth, std::unique_ptr<ElevationGrid> pGrid)
{
    const int tKey = SceneryIndex::tileKey(pWest, pSouth);
    auto tRemove = [tKey](std::list<std::pair<int, std::unique_ptr<ElevationGrid>>>& pList)
    {
        for (auto tIterator = pList.begin(); tIterator != pList.end(); ++tIterator)
        {
            if (tIterator->first == tKey)
            {
                pList.erase(tIterator);
                return;
            }
        }
    };
    tRemove(mElevations);
    tRemove(mCoarseElevations);
    mElevations.emplace_front(tKey, std::move(pGrid));
    if (mElevations.size() > scaledLimit(kMaxElevationTiles))
    {
        std::pair<int, std::unique_ptr<ElevationGrid>> tOldest = std::move(mElevations.back());
        mElevations.pop_back();
        tOldest.second->dropFineLevels(kCoarseElevationLevel);
        keepCoarseElevation(tOldest.first, std::move(tOldest.second));
    }
    std::lock_guard<std::mutex> tLock(mMutex);
    mElevationReady.insert(tKey);
}

void TerrainStore::keepCoarseElevation(int pKey, std::unique_ptr<ElevationGrid> pGrid)
{
    mCoarseElevations.emplace_front(pKey, std::move(pGrid));
    while (mCoarseElevations.size() > scaledLimit(kMaxCoarseElevationTiles))
    {
        mCoarseElevations.pop_back();
    }
    std::lock_guard<std::mutex> tLock(mMutex);
    mElevationReady.insert(pKey);
}

const ElevationGrid* TerrainStore::elevationFor(int pWest, int pSouth, int pLevel)
{
    const int tKey = SceneryIndex::tileKey(pWest, pSouth);
    auto tFind = [tKey](std::list<std::pair<int, std::unique_ptr<ElevationGrid>>>& pList) -> const ElevationGrid*
    {
        for (auto tIterator = pList.begin(); tIterator != pList.end(); ++tIterator)
        {
            if (tIterator->first == tKey)
            {
                pList.splice(pList.begin(), pList, tIterator);
                return pList.front().second.get();
            }
        }
        return nullptr;
    };
    const bool tCoarseIsEnough = pLevel >= kCoarseElevationLevel;
    if (const ElevationGrid* tFull = tFind(mElevations))
    {
        return tFull;
    }
    if (tCoarseIsEnough)
    {
        if (const ElevationGrid* tCoarse = tFind(mCoarseElevations))
        {
            return tCoarse;
        }
    }
    std::unique_ptr<ElevationGrid> tGrid = std::make_unique<ElevationGrid>();
    const Clock::time_point tStart = Clock::now();
    {
        TerrainProfile::ScopedStage tStage(TerrainProfile::Stage::loadElevation);
        if (!loadElevation(mIndex, pWest, pSouth, mCacheDirectory, tGrid.get()))
        {
            return nullptr;
        }
    }
    ++mStats.mElevationLoads;
    addLog(std::format("terrain {}: elevation from cache in {:.0f} ms ({})", SceneryIndex::tileName(pWest, pSouth),
                       millisecondsSince(tStart), tCoarseIsEnough ? "coarse" : "full"));
    if (tCoarseIsEnough)
    {
        tGrid->dropFineLevels(kCoarseElevationLevel);
        keepCoarseElevation(tKey, std::move(tGrid));
        return mCoarseElevations.front().second.get();
    }
    keepElevation(pWest, pSouth, std::move(tGrid));
    return mElevations.front().second.get();
}

void TerrainStore::buildImageFor(const TerrainImageKey& pKey, const TerrainRaster& pRaster, TerrainImage* pOut)
{
    const int tChunks = chunksPerSide(pKey.mLevel);
    const int tChunkColumn = pKey.mChunk % tChunks;
    const int tChunkRow = pKey.mChunk / tChunks;
    const bool tTouches[3][3] = {
        {tChunkRow == 0 && tChunkColumn == 0, tChunkRow == 0, tChunkRow == 0 && tChunkColumn == tChunks - 1},
        {tChunkColumn == 0, true, tChunkColumn == tChunks - 1},
        {tChunkRow == tChunks - 1 && tChunkColumn == 0, tChunkRow == tChunks - 1,
         tChunkRow == tChunks - 1 && tChunkColumn == tChunks - 1},
    };
    const double tMetersPerPixel = 111320.0 / (static_cast<double>(tChunks) * pOut->mSize);
    const int tLevel = ElevationGrid::nominalLevelFor(tMetersPerPixel);
    ElevationTiles tTiles;
    tTiles.mTiles[1][1] = elevationFor(pKey.mWest, pKey.mSouth, tLevel);
    for (int tRow = 0; tRow < 3 && tTiles.mTiles[1][1]; ++tRow)
    {
        for (int tColumn = 0; tColumn < 3; ++tColumn)
        {
            const int tWest = wrapWest(pKey.mWest + tColumn - 1);
            const int tSouth = pKey.mSouth + 1 - tRow;
            if ((tRow == 1 && tColumn == 1) || !tTouches[tRow][tColumn] || tSouth < -90 || tSouth > 89 ||
                mIndex.coverageOf(tWest, tSouth) != TileCoverage::hasDsf)
            {
                continue;
            }
            tTiles.mTiles[tRow][tColumn] = elevationFor(tWest, tSouth, tLevel);
            if (!tTiles.mTiles[tRow][tColumn] && !hasElevation(tWest, tSouth))
            {
                pOut->mMissingNeighbors.push_back(SceneryIndex::tileKey(tWest, tSouth));
            }
        }
    }
    const ElevationTiles* tElevation = tTiles.mTiles[1][1] ? &tTiles : nullptr;

    TerrainPalette tPalette;
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        const size_t tIndex = pKey.mPalette >= 0 && pKey.mPalette < static_cast<int>(mPalettes.size())
                                  ? static_cast<size_t>(pKey.mPalette)
                                  : 0;
        tPalette = mPalettes[tIndex];
    }
    const TerrainImageStyle tStyle = pKey.mStyle >= 0 && pKey.mStyle < static_cast<int>(mStyles.size())
                                         ? mStyles[pKey.mStyle]
                                         : TerrainImageStyle();
    if (pKey.mLevel >= kDetailLevel)
    {
        const TerrainTriangles* tTriangles = trianglesFor(pKey.mWest, pKey.mSouth);
        if (!tTriangles)
        {
            pOut->mRgba.clear();
            return;
        }
        const std::shared_ptr<TerrainVectors> tVectors =
            pKey.mAreas != 0 ? vectorsFor(pKey.mWest, pKey.mSouth) : nullptr;
        pRaster.buildDetailImage(*tTriangles, tChunks, pKey.mChunk, pOut->mSize, tPalette, tStyle, &pOut->mRgba,
                                 tElevation, tVectors.get(), pKey.mAreas);
        return;
    }
    const int tSpan = TerrainRaster::kSize / tChunks;
    const int tX0 = tChunkColumn * tSpan;
    const int tY0 = tChunkRow * tSpan;
    pRaster.buildImage(tX0, tY0, tX0 + tSpan, tY0 + tSpan, pOut->mSize, tPalette, tStyle, &pOut->mRgba, tElevation,
                       pKey.mAreas);
}

std::shared_ptr<TerrainVectors> TerrainStore::vectorsFor(int pWest, int pSouth)
{
    const int tKey = SceneryIndex::tileKey(pWest, pSouth);
    for (auto tIterator = mVectors.begin(); tIterator != mVectors.end(); ++tIterator)
    {
        if (tIterator->first == tKey)
        {
            mVectors.splice(mVectors.begin(), mVectors, tIterator);
            return mVectors.front().second;
        }
    }
    std::shared_ptr<TerrainVectors> tVectors = std::make_shared<TerrainVectors>();
    ++mStats.mVectorMisses[tKey];
    if (!loadVectors(mIndex, pWest, pSouth, mCacheDirectory, tVectors.get()))
    {
        if (!rasterFor(pWest, pSouth) || mVectors.empty() || mVectors.front().first != tKey)
        {
            return nullptr;
        }
        return mVectors.front().second;
    }
    mVectors.emplace_front(tKey, tVectors);
    while (mVectors.size() > scaledLimit(kMaxVectorTiles))
    {
        mVectors.pop_back();
    }
    return tVectors;
}

bool TerrainStore::loadVectors(const SceneryIndex& pIndex, int pWest, int pSouth, const std::string& pCacheDirectory,
                               TerrainVectors* pOut)
{
    const std::vector<DsfCandidate>* tCandidates = pIndex.candidatesFor(pWest, pSouth);
    if (!tCandidates || pCacheDirectory.empty())
    {
        return false;
    }
    const std::string tPath = pCacheDirectory + SceneryIndex::tileName(pWest, pSouth) + ".vectors";
    return pOut->load(tPath, cacheKey(*tCandidates));
}

void TerrainStore::forgetVectors(int pWest, int pSouth)
{
    std::lock_guard<std::mutex> tLock(mMutex);
    mVectorsServed.erase(SceneryIndex::tileKey(pWest, pSouth));
}

bool TerrainStore::serveVectorRequest()
{
    int tKey = 0;
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        uint64_t tBestFrame = 0;
        bool tFound = false;
        for (auto tIterator = mVectorRequests.begin(); tIterator != mVectorRequests.end();)
        {
            if (tIterator->second + kStaleFrames < mFrame)
            {
                tIterator = mVectorRequests.erase(tIterator);
                continue;
            }
            if (!tFound || tIterator->second > tBestFrame)
            {
                tFound = true;
                tBestFrame = tIterator->second;
                tKey = tIterator->first;
            }
            ++tIterator;
        }
        if (!tFound)
        {
            return false;
        }
        mVectorRequests.erase(tKey);
        mVectorsServed.insert(tKey);
        const auto tState = mStates.find(tKey);
        if (tState != mStates.end() && tState->second != TerrainTileState::Ready &&
            tState->second != TerrainTileState::Unknown)
        {
            return true;
        }
    }
    const std::shared_ptr<TerrainVectors> tVectors = vectorsFor(tKey % 360 - 180, tKey / 360 - 90);
    if (tVectors)
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        mVectorResults.push_back(tVectors);
    }
    return true;
}

void TerrainStore::noteImageTime(int pLevel, double pMilliseconds)
{
    if (pLevel >= 0 && pLevel < kLevelCount)
    {
        ++mImageCounts[pLevel];
        mImageMilliseconds[pLevel] += pMilliseconds;
    }
}

void TerrainStore::logImageTimes()
{
    std::string tText;
    for (int tLevel = 0; tLevel < kLevelCount; ++tLevel)
    {
        if (mImageCounts[tLevel] > 0)
        {
            tText += std::format(" L{} {} x {:.0f} ms", tLevel, mImageCounts[tLevel],
                                 mImageMilliseconds[tLevel] / mImageCounts[tLevel]);
            mImageCounts[tLevel] = 0;
            mImageMilliseconds[tLevel] = 0.0;
        }
    }
    if (!tText.empty())
    {
        addLog("terrain images (count x average):" + tText);
    }
    const std::string tStages = TerrainProfile::takeReport();
    WorkStats tStats;
    std::swap(tStats, mStats);
    if (!mDiagnostics.load())
    {
        return;
    }
    if (!tStages.empty())
    {
        addLog("terrain profile stages (count x avg ms = total):" + tStages);
    }
    auto tTiles = [](const std::map<int, int>& pCounts)
    {
        std::string tList;
        int tTotal = 0;
        for (const auto& [tKey, tCount] : pCounts)
        {
            tTotal += tCount;
            tList += std::format(" {}x{}", SceneryIndex::tileName(tKey % 360 - 180, tKey / 360 - 90), tCount);
        }
        return std::format("{}{}", tTotal, tList);
    };
    addLog(std::format("terrain profile reloads: raster {} ({:.0f} ms); triangles {} ({:.0f} ms); vectors {}; "
                       "elevation from disk {}",
                       tTiles(tStats.mRasterMisses), tStats.mRasterMissMs, tTiles(tStats.mTriangleLoads),
                       tStats.mTriangleMs, tTiles(tStats.mVectorMisses), tStats.mElevationLoads));
    std::string tWait;
    for (int tLevel = 0; tLevel < kLevelCount; ++tLevel)
    {
        const int tCount = tStats.mLatencyCount[tLevel];
        if (tCount > 0)
        {
            tWait += std::format(" L{} {}: queue {:.0f} ready {:.0f} max {:.0f} ms;", tLevel, tCount,
                                 tStats.mQueueMs[tLevel] / tCount, tStats.mReadyMs[tLevel] / tCount,
                                 tStats.mReadyMaxMs[tLevel]);
        }
    }
    addLog(std::format("terrain profile waits (avg from first request):{} background tiles {} ({:.0f} ms), "
                       "screen blocked by background {} times (<= {:.0f} ms)",
                       tWait, tStats.mBackgroundCount, tStats.mBackgroundMs, tStats.mBlockedCount, tStats.mBlockedMs));
}

void TerrainStore::workerLoop()
{
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        mStatus = "Indexing scenery...";
    }
    std::error_code tError;
    std::filesystem::create_directories(mCacheDirectory, tError);
    const Clock::time_point tStart = Clock::now();
    mIndex.build(mXPlaneDirectory);
    mIndexReady.store(true);
    addLog(std::format("terrain index: {} DSF files in {} packs, {} tiles ({:.1f} s)", mIndex.fileCount(),
                       mIndex.packCount(), mIndex.tileCount(),
                       std::chrono::duration<double>(Clock::now() - tStart).count()));
    addLog(mIndex.worldMapReport());
    mCacheGrew = true;

    while (true)
    {
        TerrainImageKey tKey;
        Clock::time_point tPickedFirst;
        bool tIdle = false;
        {
            std::lock_guard<std::mutex> tLock(mMutex);
            mHasInFlight = false;
            tIdle = mRequests.empty() && mVectorRequests.empty();
        }
        if (serveVectorRequest())
        {
            continue;
        }
        if (serveBackgroundTile())
        {
            continue;
        }
        if (tIdle)
        {
            logImageTimes();
            if (mCacheGrew || mTrimWanted.exchange(false))
            {
                trimCache();
            }
        }
        {
            std::unique_lock<std::mutex> tLock(mMutex);
            mStatus.clear();
            mWake.wait(tLock,
                       [this]()
                       {
                           const bool tBackground = mRequests.empty() && (!mPrecache.empty() || !mPrefetch.empty());
                           return mStopping || tBackground || !mVectorRequests.empty() || mTrimWanted.load() ||
                                  (!mRequests.empty() && mResults.size() < kMaxQueuedResults);
                       });
            if (mStopping)
            {
                return;
            }
            if (!pickRequest(&tKey))
            {
                continue;
            }
            const auto tState = mStates.find(SceneryIndex::tileKey(tKey.mWest, tKey.mSouth));
            if (tState != mStates.end() && tState->second != TerrainTileState::Ready &&
                tState->second != TerrainTileState::Unknown)
            {
                continue;
            }
            mHasInFlight = true;
            mInFlight = tKey;
            tPickedFirst = mPickedFirst;
        }
        const double tQueueMs = millisecondsSince(tPickedFirst);

        const TerrainRaster* tRaster = rasterFor(tKey.mWest, tKey.mSouth);
        if (!tRaster)
        {
            continue;
        }
        TerrainImage tImage;
        tImage.mKey = tKey;
        tImage.mSize = imageSize(tKey.mLevel);
        const Clock::time_point tImageStart = Clock::now();
        buildImageFor(tKey, *tRaster, &tImage);
        if (tImage.mRgba.empty())
        {
            continue;
        }
        noteImageTime(tKey.mLevel, millisecondsSince(tImageStart));
        {
            TerrainProfile::ScopedStage tStage(TerrainProfile::Stage::Bleed);
            TerrainRaster::bleedTransparent(tImage.mSize, &tImage.mRgba);
        }
        if (tKey.mLevel >= 0 && tKey.mLevel < kLevelCount)
        {
            const double tReadyMs = millisecondsSince(tPickedFirst);
            ++mStats.mLatencyCount[tKey.mLevel];
            mStats.mQueueMs[tKey.mLevel] += tQueueMs;
            mStats.mReadyMs[tKey.mLevel] += tReadyMs;
            mStats.mReadyMaxMs[tKey.mLevel] = std::max(mStats.mReadyMaxMs[tKey.mLevel], tReadyMs);
        }

        std::lock_guard<std::mutex> tLock(mMutex);
        mResults.push_back(std::move(tImage));
    }
}
