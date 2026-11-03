// BackgroundJob.h
// Tek seferlik arka plan isi: bir fonksiyonu ayri bir thread'de calistirir,
// ana thread her karede bitip bitmedigine bakar. X-Plane'e bagimli DEGIL.
//
// Kurallar (X-Plane): arka plan thread'inde hicbir XPLM fonksiyonu (Log
// dahil) cagrilmaz. Isin sonucu, Poll() true dondukten sonra ana thread'de
// okunur (join, yazilanlarin gorunur olmasini garanti eder).
//
//   mJob.Start([this]() { mResult = AgirIs(mJob.CancelFlag()); });
//   ...her karede:  if (mJob.Poll()) { /* mResult kullanilabilir */ }
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

    // Is zaten calisiyorsa false
    bool Start(std::function<void()> pWork);

    // Ana thread'den: is bu cagrida bittiyse true (bir kez)
    bool Poll();

    bool IsRunning() const;
    void Cancel();  // isten "birak" istenir; is CancelFlag'e bakmali
    void Wait();    // bitene kadar bekle (kapanista)

    const std::atomic<bool>& CancelFlag() const;

private:
    std::thread mThread;
    std::atomic<bool> mFinished{false};
    std::atomic<bool> mCancel{false};
    bool mRunning = false;
};
