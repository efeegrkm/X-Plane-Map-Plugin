// Tek seferlik arka plan isi: bir fonksiyonu ayri bir thread'de calistirir,
// ana thread her karede bitip bitmedigine bakar. X-Plane'e bagimli DEGIL.
// Kural: thread icinde hicbir XPLM cagrisi yapilmaz; sonuc poll() true olunca
// ana thread'de okunur (join gorunurlugu garanti eder).
#pragma once

#include <atomic>
#include <functional>
#include <thread>

class BackgroundJob
{
public:
    BackgroundJob() = default;
    ~BackgroundJob();  // calisan is varsa iptal edip bekler

    BackgroundJob(const BackgroundJob&) = delete;
    BackgroundJob& operator=(const BackgroundJob&) = delete;

    bool start(std::function<void()> pWork);

    // Ana thread'den cagrilir; is bu cagrida bittiyse true (bir kez)
    bool poll();

    bool isRunning() const;
    void cancel();  // isten "birak" istenir; is cancelFlag'e bakmali
    void wait();    // bitene kadar bekle (kapanista)

    const std::atomic<bool>& cancelFlag() const;

private:
    std::thread mThread;
    std::atomic<bool> mFinished{false};
    std::atomic<bool> mCancel{false};
    bool mRunning = false;
};
