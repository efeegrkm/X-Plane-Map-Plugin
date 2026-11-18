// Bir isi ayri bir thread'de calistirir; ana thread her karede bitip bitmedigine bakar.
#ifndef PRIME_MAP_SRC_BACKGROUNDJOB_H
#define PRIME_MAP_SRC_BACKGROUNDJOB_H

#include <atomic>
#include <functional>
#include <thread>

class BackgroundJob
{
public:
    BackgroundJob() = default;
    ~BackgroundJob();

    BackgroundJob(const BackgroundJob&) = delete;
    BackgroundJob& operator=(const BackgroundJob&) = delete;

    bool start(std::function<void()> pWork);

    bool poll();

    bool isRunning() const;
    void cancel();
    void wait();

    const std::atomic<bool>& cancelFlag() const;

private:
    std::thread mThread;
    std::atomic<bool> mFinished{false};
    std::atomic<bool> mCancel{false};
    bool mRunning = false;
};
#endif
