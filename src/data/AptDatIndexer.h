// AptDatIndexer.h
// Acilista tum apt.dat dosyalarini BIR KEZ tarayip her havalimaninin ozetini
// ve dosyadaki bayt araligini cikarir. X-Plane'e bagimli DEGIL; arka plan
// thread'inde calisir.
//
// Neden: Global Airports apt.dat'i ~325 MB (37 bin havalimani, 7 milyon
// dugum). Hepsini ayristirip bellekte tutmak yerine burada sadece ozet
// (pistler, kapsayan kutu, kule) cikarilir; ayrinti gerektiginde dosyada
// havalimaninin baslangicina dogrudan atlanip sadece o kisim okunur
// (AirportDetailStore).
//
// Disk onbellegi: tarama sonucu <onbellek>/airport_index.bin dosyasina
// yazilir. Anahtar = her apt.dat'in yolu + boyutu + degisiklik zamani; sahne
// eklenir, cikarilir, sirasi degisir veya guncellenirse anahtar tutmaz ve
// dosyalar yeniden taranir. Onbellek dosyasini silmek bir sey bozmaz.
//
// Dosya sirasi (X-Plane'in kurali, ayni kimlik icin ONCE gelen kazanir;
// boylece ozel sahnedeki havalimani Global Airports'takini tamamen ezer):
//   1. Custom Scenery/scenery_packs.ini'deki etkin paketler, yukaridan asagi,
//      her birinin "Earth nav data/apt.dat"i
//   2. Resources/default scenery/default apt dat/Earth nav data/apt.dat
#pragma once

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "data/AirportGeometry.h"

// Bir dosyanin satirlarini sirayla alip havalimani ozetleri cikaran tarayici.
// Dosyadan bagimsiz: testlerde metinle beslenir.
class AptDatScanner
{
public:
    // pSeen: onceki dosyalardan alinmis kimlikler (bunlar atlanir, yenileri eklenir)
    AptDatScanner(int pFileIndex, std::unordered_set<std::string>& pSeen, std::vector<AirportGeometry>& pOut);

    // pOffset: satirin dosyadaki ilk bayti
    void AddLine(std::string_view pLine, uint64_t pOffset);
    void Finish(uint64_t pFileSize);  // dosya bitti: son havalimanini kapat

    int AirportCount() const;
    int RunwayCount() const;
    int NodeCount() const;

private:
    void Close(uint64_t pEndOffset);
    void Extend(const LatLon& pPoint);

    int mFileIndex;
    std::unordered_set<std::string>& mSeen;
    std::vector<AirportGeometry>& mOut;

    AirportGeometry mCurrent;
    bool mOpen = false;       // bir havalimaninin icindeyiz
    bool mSkip = false;       // bu kimlik onceki bir dosyadan zaten alindi
    bool mHasBounds = false;
    bool mHasDatumLat = false;
    bool mHasDatumLon = false;
    int mAirportCount = 0;
    int mRunwayCount = 0;
    int mNodeCount = 0;
};

class AptDatIndexer : public IAirportGeometrySource
{
public:
    const char* Name() const override;
    void SetCacheDirectory(const std::string& pDirectory) override;
    bool LoadAll(const std::string& pXPlaneDirectory, const std::atomic<bool>& pCancel, AirportIndex& pOut,
                 std::string& pOutReport) override;

    // Onbellek dosyasi: anahtar tutarsa pOut'u doldurup true. Testler icin acik.
    static bool LoadCache(const std::string& pPath, const std::string& pKey, AirportIndex& pOut);
    static bool SaveCache(const std::string& pPath, const std::string& pKey, const AirportIndex& pIndex);
    // Dosya listesinin anahtari (yol | boyut | degisiklik zamani)
    static std::string CacheKey(const std::vector<std::string>& pFiles);

    // Okunacak apt.dat yollari, oncelik sirasiyla (var olup olmadigina bakmaz)
    static std::vector<std::string> AptFiles(const std::string& pXPlaneDirectory);

private:
    std::string mCacheDirectory;

    static bool ScanFile(const std::string& pPath, AptDatScanner& pScanner, const std::atomic<bool>& pCancel);
};
