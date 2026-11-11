// BackgroundJob.cpp
#include "BackgroundJob.h"

#include <utility>

BackgroundJob::~BackgroundJob()
{
    Cancel();
    Wait();
}

bool BackgroundJob::Start(std::function<void()> pWork)
{
    if (mRunning)
    {
        return false;
    }
    if (mThread.joinable())
    {
        mThread.join();  // onceki bitmis isin thread'i
    }
    mRunning = true;
    mFinished.store(false);
    mCancel.store(false);
    mThread = std::thread(
        [this, tWork = std::move(pWork)]()
        {
            tWork();
            mFinished.store(true);
        });
    return true;
}

bool BackgroundJob::Poll()
{
    if (!mRunning || !mFinished.load())
    {
        return false;
    }
    mThread.join();
    mRunning = false;
    return true;
}

bool BackgroundJob::IsRunning() const
{
    return mRunning;
}

void BackgroundJob::Cancel()
{
    mCancel.store(true);
}

void BackgroundJob::Wait()
{
    if (mThread.joinable())
    {
        mThread.join();
    }
    mRunning = false;
}

const std::atomic<bool>& BackgroundJob::CancelFlag() const
{
    return mCancel;
}
