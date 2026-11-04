// AirportDetailStore.cpp
#include "data/AirportDetailStore.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>

#include "data/AirportGeometryStore.h"
#include "data/AptDatParser.h"

namespace
{

const double kSlowLoadMs = 100.0;  // bundan uzun suren yuklemeler log'a yazilir

}  // namespace

AirportDetailStore::AirportDetailStore(const AirportGeometryStore& pSummaries)
    : mSummaries(pSummaries)
{
}

AirportDetailStore::~AirportDetailStore()
{
    Stop();
}

void AirportDetailStore::Start()
{
    if (mThread.joinable())
    {
        return;
    }
    mStopping = false;
    mThread = std::thread(
        [this]()
        {
            WorkerLoop();
        });
}

void AirportDetailStore::Stop()
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

void AirportDetailStore::BeginFrame()
{
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
    EvictUnused();
}

const LoadedAirport* AirportDetailStore::Use(int pAirport)
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

void AirportDetailStore::EndFrame()
{
    std::lock_guard<std::mutex> tLock(mMutex);
    // Kuyrugu bu karenin isteklerine gore yeniden kur: ekrandan cikanlar
    // beklemeye devam etmesin, yeni gorunenler one gecsin
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
            continue;  // isci su an bunu okuyor
        }
        const AirportGeometry& tSummary = mSummaries.At(tAirport);
        Request tRequest;
        tRequest.mAirport = tAirport;
        tRequest.mPath = mSummaries.FilePath(tSummary.mLocation.mFile);
        tRequest.mOffset = tSummary.mLocation.mOffset;
        tRequest.mLength = tSummary.mLocation.mLength;
        tRequest.mReference = tSummary.mReference;
        tRequest.mId = tSummary.mId;
        tQueue.push_back(std::move(tRequest));
    }
    // Kuyruktan cikarilanlar artik "yolda" degil
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

bool AirportDetailStore::IsWaiting(int pAirport) const
{
    return mWantedSet.count(pAirport) > 0;
}

int AirportDetailStore::PendingCount() const
{
    return mPendingCount;
}

int AirportDetailStore::LoadedCount() const
{
    return static_cast<int>(mLoaded.size());
}

std::string AirportDetailStore::TakeLogLines()
{
    std::lock_guard<std::mutex> tLock(mMutex);
    std::string tLog;
    tLog.swap(mLog);
    return tLog;
}

void AirportDetailStore::EvictUnused()
{
    while (mLoaded.size() > kMaxLoaded)
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
            return;  // hepsi su an ekranda
        }
        mLoaded.erase(tOldest);
    }
}

std::unique_ptr<LoadedAirport> AirportDetailStore::Load(const Request& pRequest, std::string& pLog) const
{
    const auto tStart = std::chrono::steady_clock::now();
    std::ifstream tFile(pRequest.mPath, std::ios::binary);
    if (!tFile)
    {
        pLog += std::format("airport {}: cannot open {}\n", pRequest.mId, pRequest.mPath);
        return nullptr;
    }
    // Dosyanin icinde havalimaninin baslangicina dogrudan atla
    std::string tText(pRequest.mLength, '\0');
    tFile.seekg(static_cast<std::streamoff>(pRequest.mOffset));
    tFile.read(tText.data(), static_cast<std::streamsize>(tText.size()));
    tText.resize(static_cast<size_t>(tFile.gcount()));

    auto tLoaded = std::make_unique<LoadedAirport>();
    tLoaded->mDetail = AptDatParser::ParseAirport(tText);
    if (tLoaded->mDetail.mId != pRequest.mId)
    {
        // Dosya acilistan sonra degismis olabilir: yanlis havalimanini cizme
        pLog += std::format("airport {}: file changed since indexing ({} found at offset {})\n", pRequest.mId,
                            tLoaded->mDetail.mId, pRequest.mOffset);
        return nullptr;
    }
    tLoaded->mShapes = Shapes::Build(tLoaded->mDetail, pRequest.mReference);
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

void AirportDetailStore::WorkerLoop()
{
    // ARKA PLAN THREAD'i: burada XPLM fonksiyonu cagrilmaz
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
        std::unique_ptr<LoadedAirport> tLoaded = Load(tRequest, tLog);
        std::lock_guard<std::mutex> tLock(mMutex);
        mInFlight.erase(tRequest.mAirport);
        mResults.push_back({tRequest.mAirport, std::move(tLoaded)});
        mLog += tLog;
    }
}
