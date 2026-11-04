// AirportGeometryStore.h
// Havalimani OZETLERININ (pistler, kapsayan kutu, dosyadaki yer) deposu:
//   - kaynagi (IAirportGeometrySource) arka plan thread'inde calistirir,
//   - bitince sonucu ana thread'de devralir (Poll),
//   - havalimanlarini 1x1 derece izgaraya koyar, ekrandakileri sorgulatir.
// Ayrintilar (yuzeyler, cizgiler...) AirportDetailStore'da. X-Plane'e bagimli DEGIL.
#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "GeoGrid.h"
#include "data/AirportGeometry.h"

class AirportGeometryStore
{
public:
    explicit AirportGeometryStore(std::unique_ptr<IAirportGeometrySource> pSource);
    ~AirportGeometryStore();  // calisan okuma varsa iptal edip bekler

    AirportGeometryStore(const AirportGeometryStore&) = delete;
    AirportGeometryStore& operator=(const AirportGeometryStore&) = delete;

    // Okumayi arka planda baslatir (bir kez)
    void StartLoading(const std::string& pXPlaneDirectory);

    // Ana thread'den her karede. Okuma bu cagrida bittiyse true doner (bir kez);
    // o andan sonra IsReady() true'dur. Rapor: LastReport().
    bool Poll();

    bool IsLoading() const;
    bool IsReady() const;
    bool LastLoadSucceeded() const;
    const std::string& LastReport() const;
    const char* SourceName() const;

    int Count() const;
    int RunwayCount() const;
    const AirportGeometry& At(int pIndex) const;
    const AirportGeometry* Find(const std::string& pId) const;  // baslik kimligiyle
    const std::string& FilePath(int pFile) const;  // AptFileLocation::mFile -> yol

    // pBounds'un degdigi hucrelerdeki havalimanlarinin indekslerini pOut'a ekler.
    // Havalimani konumu = referans noktasi; havalimani bundan birkac km
    // tasabilecegi icin cagiran sinirlari biraz genisletmeli.
    void Query(const GeoBounds& pBounds, std::vector<int>& pOut) const;

private:
    void TakeResult();

    std::unique_ptr<IAirportGeometrySource> mSource;
    std::thread mThread;
    std::atomic<bool> mCancel{false};
    std::atomic<bool> mFinished{false};

    // Arka plan thread'inin yazdiklari (mFinished true olduktan sonra okunur)
    AirportIndex mPending;
    std::string mPendingReport;
    bool mPendingSucceeded = false;

    // Ana thread'in verisi
    std::vector<AirportGeometry> mAirports;
    std::vector<std::string> mFiles;
    std::unordered_map<std::string, int> mById;
    GeoGrid mGrid;
    std::string mReport;
    bool mLoading = false;
    bool mReady = false;
    bool mSucceeded = false;
    int mRunwayCount = 0;
};
