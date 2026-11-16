
#include "BackgroundJob.h"

#include <utility>

BackgroundJob::~BackgroundJob()
{
    cancel();
    wait();
}

bool BackgroundJob::start(std::function<void()> pWork)
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

bool BackgroundJob::poll()
{
    if (!mRunning || !mFinished.load())
    {
        return false;
    }
    mThread.join();
    mRunning = false;
    return true;
}

bool BackgroundJob::isRunning() const
{
    return mRunning;
}

void BackgroundJob::cancel()
{
    mCancel.store(true);
}

void BackgroundJob::wait()
{
    if (mThread.joinable())
    {
        mThread.join();
    }
    mRunning = false;
}

const std::atomic<bool>& BackgroundJob::cancelFlag() const
{
    return mCancel;
}
