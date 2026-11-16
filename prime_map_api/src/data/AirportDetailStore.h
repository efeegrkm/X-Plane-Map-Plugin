// Ekrandaki havalimanlarinin AYRINTISINI (yuzeyler, cizgiler, tabelalar...)
// arka planda okuyup cizime hazirlayan depo. X-Plane'e bagimli DEGIL.
//
// akis (ana thread):
//   cizimde, herhangi bir katman:  use(i) -> hazirsa verir, degilse istek listesine yazar
//   her karede bir kez (flight loop): update() -> istekleri isciye gonderir,
//       biten isleri alir. Boylece birden fazla katman ayni depoyu kullanabilir.
//
// Isci thread'i: havalimaninin dosyadaki bayt araligina atlar (AptFileLocation),
// satirlari okur, AptDatParser ile ayristirir, AirportShapes ile ucgenler.
// Bellekte en fazla kMaxLoaded havalimani; en uzun suredir kullanilmayan silinir.
#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "data/AirportDetail.h"
#include "data/AirportShapes.h"

class AirportGeometryStore;

struct LoadedAirport
{
    AirportDetail mDetail;
    AirportShapes mShapes;
    double mLoadMs = 0.0;
};

class AirportDetailStore
{
public:
    static const size_t kMaxLoaded = 64;

    explicit AirportDetailStore(const AirportGeometryStore& pSummaries);
    ~AirportDetailStore();

    AirportDetailStore(const AirportDetailStore&) = delete;
    AirportDetailStore& operator=(const AirportDetailStore&) = delete;

    // Bellekte tutulan en fazla havalimani (varsayilan kMaxLoaded). Acik harita
    // sayisina gore buyutulur (iki harita birbirinin havalimanlarini atmasin).
    void setMaxLoaded(size_t pCount);

    void start();  // isci thread'ini baslatir (bir kez)
    void stop();   // o anki isi bitirip durur

    // Her karede bir kez: istekleri isciye gonder, biten isleri al, eskileri sil
    void update();
    // Ayrinti hazirsa verir (ve bu karede kullanildi diye isaretler); degilse
    // istek listesine ekler ve nullptr doner. Siralama onemli: once istenen once okunur.
    const LoadedAirport* use(int pAirport);

    bool isWaiting(int pAirport) const;  // istendi ama henuz hazir degil
    int pendingCount() const;            // bu karede istenip bekleyen
    int loadedCount() const;
    std::string takeLogLines();          // isci raporlari (ana thread loglar)

private:
    struct Request
    {
        int mAirport = -1;
        std::string mPath;
        uint64_t mOffset = 0;
        uint32_t mLength = 0;
        LatLon mReference;
        std::string mId;
    };

    struct Result
    {
        int mAirport = -1;
        std::unique_ptr<LoadedAirport> mLoaded;
    };

    struct Entry
    {
        std::unique_ptr<LoadedAirport> mLoaded;
        uint64_t mLastFrame = 0;
    };

    void submitWanted();  // istek listesini isci kuyruguna yazar
    void workerLoop();
    std::unique_ptr<LoadedAirport> load(const Request& pRequest, std::string& pLog) const;
    void evictUnused();

    const AirportGeometryStore& mSummaries;

    // Ana thread
    std::unordered_map<int, Entry> mLoaded;
    std::unordered_set<int> mFailed;     // okunamayanlar: tekrar istenmez
    std::vector<int> mWanted;            // bu karede istenen (sirali)
    std::unordered_set<int> mWantedSet;
    uint64_t mFrame = 0;
    int mPendingCount = 0;
    size_t mMaxLoaded = kMaxLoaded;

    // Ana thread <-> isci (mMutex ile)
    mutable std::mutex mMutex;
    std::condition_variable mWake;
    std::deque<Request> mQueue;
    std::unordered_set<int> mInFlight;   // kuyrukta veya isleniyor
    std::vector<Result> mResults;
    std::string mLog;
    bool mStopping = false;
    std::thread mThread;
};
