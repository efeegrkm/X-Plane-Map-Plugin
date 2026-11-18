#include "data/AirportDetailStore.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>

#include "data/AirportGeometryStore.h"
#include "data/AptDatParser.h"

namespace
{
const double kSlowLoadMs = 100.0;
}

AirportDetailStore::AirportDetailStore(const AirportGeometryStore& pSummaries)
    : mSummaries(pSummaries)
{
}

AirportDetailStore::~AirportDetailStore()
{
    stop();
}

void AirportDetailStore::setMaxLoaded(size_t pCount)
{
    mMaxLoaded = pCount < kMaxLoaded ? kMaxLoaded : pCount;
}

void AirportDetailStore::start()
{
    if (mThread.joinable())
    {
        return;
    }
    mStopping = false;
    mThread = std::thread(
        [this]()
        {
            workerLoop();
        });
}

void AirportDetailStore::stop()
{
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        mStopping = true;
        mQueue.clear();
    }
    mWake.notify_all();
    if (mThread.joinable())
    {
        mThread.join();
    }
}

void AirportDetailStore::update()
{
    submitWanted();
    ++mFrame;
    mWanted.clear();
    mWantedSet.clear();
    std::vector<Result> tResults;
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        tResults.swap(mResults);
    }
    for (Result& tResult : tResults)
    {
        if (!tResult.mLoaded)
        {
            mFailed.insert(tResult.mAirport);
            continue;
        }
        Entry& tEntry = mLoaded[tResult.mAirport];
        tEntry.mLoaded = std::move(tResult.mLoaded);
        tEntry.mLastFrame = mFrame;
    }
    evictUnused();
}

const LoadedAirport* AirportDetailStore::use(int pAirport)
{
    const auto tFound = mLoaded.find(pAirport);
    if (tFound != mLoaded.end())
    {
        tFound->second.mLastFrame = mFrame;
        return tFound->second.mLoaded.get();
    }
    if (mFailed.count(pAirport) > 0)
    {
        return nullptr;
    }
    if (mWantedSet.insert(pAirport).second)
    {
        mWanted.push_back(pAirport);
    }
    return nullptr;
}

void AirportDetailStore::submitWanted()
{
    std::lock_guard<std::mutex> tLock(mMutex);
    std::deque<Request> tQueue;
    for (int tAirport : mWanted)
    {
        const auto tQueued = std::find_if(mQueue.begin(), mQueue.end(), [tAirport](const Request& pRequest)
                                          {
                                              return pRequest.mAirport == tAirport;
                                          });
        if (tQueued != mQueue.end())
        {
            tQueue.push_back(*tQueued);
            continue;
        }
        if (mInFlight.count(tAirport) > 0)
        {
            continue;
        }
        const AirportGeometry& tSummary = mSummaries.at(tAirport);
        Request tRequest;
        tRequest.mAirport = tAirport;
        tRequest.mPath = mSummaries.filePath(tSummary.mLocation.mFile);
        tRequest.mOffset = tSummary.mLocation.mOffset;
        tRequest.mLength = tSummary.mLocation.mLength;
        tRequest.mReference = tSummary.mReference;
        tRequest.mId = tSummary.mId;
        tQueue.push_back(std::move(tRequest));
    }
    for (const Request& tOld : mQueue)
    {
        if (!mWantedSet.count(tOld.mAirport))
        {
            mInFlight.erase(tOld.mAirport);
        }
    }
    mQueue.swap(tQueue);
    for (const Request& tRequest : mQueue)
    {
        mInFlight.insert(tRequest.mAirport);
    }
    mPendingCount = static_cast<int>(mWanted.size());
    if (!mQueue.empty())
    {
        mWake.notify_one();
    }
}

bool AirportDetailStore::isWaiting(int pAirport) const
{
    return mWantedSet.count(pAirport) > 0;
}

int AirportDetailStore::pendingCount() const
{
    return mPendingCount;
}

int AirportDetailStore::loadedCount() const
{
    return static_cast<int>(mLoaded.size());
}

std::string AirportDetailStore::takeLogLines()
{
    std::lock_guard<std::mutex> tLock(mMutex);
    std::string tLog;
    tLog.swap(mLog);
    return tLog;
}

void AirportDetailStore::evictUnused()
{
    while (mLoaded.size() > mMaxLoaded)
    {
        auto tOldest = mLoaded.end();
        for (auto tIterator = mLoaded.begin(); tIterator != mLoaded.end(); ++tIterator)
        {
            if (tIterator->second.mLastFrame < mFrame &&
                (tOldest == mLoaded.end() || tIterator->second.mLastFrame < tOldest->second.mLastFrame))
            {
                tOldest = tIterator;
            }
        }
        if (tOldest == mLoaded.end())
        {
            return;
        }
        mLoaded.erase(tOldest);
    }
}

std::unique_ptr<LoadedAirport> AirportDetailStore::load(const Request& pRequest, std::string& pLog) const
{
    const auto tStart = std::chrono::steady_clock::now();
    std::ifstream tFile(pRequest.mPath, std::ios::binary);
    if (!tFile)
    {
        pLog += std::format("airport {}: cannot open {}\n", pRequest.mId, pRequest.mPath);
        return nullptr;
    }
    std::string tText(pRequest.mLength, '\0');
    tFile.seekg(static_cast<std::streamoff>(pRequest.mOffset));
    tFile.read(tText.data(), static_cast<std::streamsize>(tText.size()));
    tText.resize(static_cast<size_t>(tFile.gcount()));

    auto tLoaded = std::make_unique<LoadedAirport>();
    tLoaded->mDetail = AptDatParser::parseAirport(tText);
    if (tLoaded->mDetail.mId != pRequest.mId)
    {
        pLog += std::format("airport {}: file changed since indexing ({} found at offset {})\n", pRequest.mId,
                            tLoaded->mDetail.mId, pRequest.mOffset);
        return nullptr;
    }
    tLoaded->mShapes = Shapes::build(tLoaded->mDetail, pRequest.mReference);
    tLoaded->mLoadMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tStart).count();

    const AirportShapes& tShapes = tLoaded->mShapes;
    if (tShapes.mFailedPolygons > 0 || tLoaded->mDetail.mSkippedLineCount > 0 || tLoaded->mLoadMs > kSlowLoadMs)
    {
        pLog += std::format("airport {}: {} pavements ({} not triangulated), {} lines, {} skipped rows, {:.0f} ms\n",
                            pRequest.mId, tLoaded->mDetail.mPavements.size(), tShapes.mFailedPolygons,
                            tShapes.mLines.size(), tLoaded->mDetail.mSkippedLineCount, tLoaded->mLoadMs);
        for (const std::string& tExample : tLoaded->mDetail.mSkippedExamples)
        {
            pLog += std::format("  skipped row: {}\n", tExample);
        }
    }
    return tLoaded;
}

void AirportDetailStore::workerLoop()
{
    while (true)
    {
        Request tRequest;
        {
            std::unique_lock<std::mutex> tLock(mMutex);
            mWake.wait(tLock,
                       [this]()
                       {
                           return mStopping || !mQueue.empty();
                       });
            if (mStopping)
            {
                return;
            }
            tRequest = std::move(mQueue.front());
            mQueue.pop_front();
        }
        std::string tLog;
        std::unique_ptr<LoadedAirport> tLoaded = load(tRequest, tLog);
        std::lock_guard<std::mutex> tLock(mMutex);
        mInFlight.erase(tRequest.mAirport);
        mResults.push_back({tRequest.mAirport, std::move(tLoaded)});
        mLog += tLog;
    }
}
