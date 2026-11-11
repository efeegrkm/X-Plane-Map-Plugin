// AirportGeometryStore.cpp
#include "data/AirportGeometryStore.h"

#include <utility>

AirportGeometryStore::AirportGeometryStore(std::unique_ptr<IAirportGeometrySource> pSource)
    : mSource(std::move(pSource))
{
}

AirportGeometryStore::~AirportGeometryStore()
{
    mCancel.store(true);
    if (mThread.joinable())
    {
        mThread.join();
    }
}

void AirportGeometryStore::StartLoading(const std::string& pXPlaneDirectory, const std::string& pCacheDirectory)
{
    if (mLoading || mReady || !mSource)
    {
        return;
    }
    mSource->SetCacheDirectory(pCacheDirectory);
    mLoading = true;
    mFinished.store(false);
    mThread = std::thread(
        [this, pXPlaneDirectory]()
        {
            // ARKA PLAN THREAD'i: burada XPLM fonksiyonu cagrilmaz
            mPendingSucceeded = mSource->LoadAll(pXPlaneDirectory, mCancel, mPending, mPendingReport);
            mFinished.store(true);
        });
}

bool AirportGeometryStore::Poll()
{
    if (!mLoading || !mFinished.load())
    {
        return false;
    }
    mThread.join();  // thread bitti; join ayrica yazdiklarinin gorunur olmasini garanti eder
    mLoading = false;
    TakeResult();
    return true;
}

void AirportGeometryStore::TakeResult()
{
    mAirports = std::move(mPending.mAirports);
    mFiles = std::move(mPending.mFiles);
    mPending = AirportIndex();
    mReport = std::move(mPendingReport);
    mSucceeded = mPendingSucceeded;

    mById.clear();
    mRunwayCount = 0;
    std::vector<GridItem> tItems;
    tItems.reserve(mAirports.size());
    for (int tIndex = 0; tIndex < Count(); ++tIndex)
    {
        AirportGeometry& tAirport = mAirports[tIndex];
        if (tAirport.mReference.mLat == 0.0 && tAirport.mReference.mLon == 0.0 && !tAirport.mRunways.empty())
        {
            // Kaynak referans vermediyse: pist uclarinin ortalamasi
            LatLon tSum = {0.0, 0.0};
            for (const Runway& tRunway : tAirport.mRunways)
            {
                for (const RunwayEnd& tEnd : tRunway.mEnds)
                {
                    tSum.mLat += tEnd.mPosition.mLat;
                    tSum.mLon += tEnd.mPosition.mLon;
                }
            }
            const double tPoints = 2.0 * tAirport.mRunways.size();
            tAirport.mReference = {tSum.mLat / tPoints, tSum.mLon / tPoints};
        }
        mById.emplace(tAirport.mId, tIndex);
        mRunwayCount += static_cast<int>(tAirport.mRunways.size());
        tItems.push_back({static_cast<uint32_t>(tIndex), static_cast<float>(tAirport.mReference.mLat),
                          static_cast<float>(tAirport.mReference.mLon)});
    }
    mGrid.Build(tItems);
    mReady = true;
}

bool AirportGeometryStore::IsLoading() const
{
    return mLoading;
}

bool AirportGeometryStore::IsReady() const
{
    return mReady;
}

bool AirportGeometryStore::LastLoadSucceeded() const
{
    return mSucceeded;
}

const std::string& AirportGeometryStore::LastReport() const
{
    return mReport;
}

const char* AirportGeometryStore::SourceName() const
{
    return mSource ? mSource->Name() : "(no source)";
}

int AirportGeometryStore::Count() const
{
    return static_cast<int>(mAirports.size());
}

int AirportGeometryStore::RunwayCount() const
{
    return mRunwayCount;
}

const AirportGeometry& AirportGeometryStore::At(int pIndex) const
{
    return mAirports[pIndex];
}

const AirportGeometry* AirportGeometryStore::Find(const std::string& pId) const
{
    const auto tFound = mById.find(pId);
    return tFound == mById.end() ? nullptr : &mAirports[tFound->second];
}

const std::string& AirportGeometryStore::FilePath(int pFile) const
{
    static const std::string kNone;
    return (pFile >= 0 && pFile < static_cast<int>(mFiles.size())) ? mFiles[pFile] : kNone;
}

void AirportGeometryStore::Query(const GeoBounds& pBounds, std::vector<int>& pOut) const
{
    if (mReady)
    {
        mGrid.Query(pBounds, pOut);
    }
}
