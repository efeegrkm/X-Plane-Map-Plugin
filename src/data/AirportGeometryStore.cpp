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

void AirportGeometryStore::StartLoading(const std::string& pXPlaneDirectory)
{
    if (mLoading || mReady || !mSource)
    {
        return;
    }
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
    mAirports = std::move(mPending);
    mPending.clear();
    mReport = std::move(mPendingReport);
    mSucceeded = mPendingSucceeded;

    mByIcao.clear();
    mRunwayCount = 0;
    std::vector<GridItem> tItems;
    tItems.reserve(mAirports.size());
    for (int tIndex = 0; tIndex < Count(); ++tIndex)
    {
        const AirportGeometry& tAirport = mAirports[tIndex];
        mByIcao.emplace(tAirport.mIcao, tIndex);
        mRunwayCount += static_cast<int>(tAirport.mRunways.size());

        // Izgara konumu: pist uclarinin ortalamasi
        double tLat = 0.0;
        double tLon = 0.0;
        int tPoints = 0;
        for (const Runway& tRunway : tAirport.mRunways)
        {
            for (const RunwayEnd& tEnd : tRunway.mEnds)
            {
                tLat += tEnd.mPosition.mLat;
                tLon += tEnd.mPosition.mLon;
                ++tPoints;
            }
        }
        if (tPoints > 0)
        {
            tItems.push_back({static_cast<uint32_t>(tIndex), static_cast<float>(tLat / tPoints),
                              static_cast<float>(tLon / tPoints)});
        }
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
    return mSource ? mSource->Name() : "(kaynak yok)";
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

const AirportGeometry* AirportGeometryStore::Find(const std::string& pIcao) const
{
    const auto tFound = mByIcao.find(pIcao);
    return tFound == mByIcao.end() ? nullptr : &mAirports[tFound->second];
}

void AirportGeometryStore::Query(const GeoBounds& pBounds, std::vector<int>& pOut) const
{
    if (mReady)
    {
        mGrid.Query(pBounds, pOut);
    }
}
