// AptDatReader.h
// *** GECICI *** apt.dat okuyucusu. EKIP MOTORU GELINCE DEGISTIRILECEK.
// (Nasil: data/AirportGeometry.h'nin basindaki talimat.)
//
// Sadece gereken kadarini okur (X-Plane 11 apt.dat 1100+ bicimi):
//   1 / 16 / 17  : havalimani / deniz ussu / heliport basligi
//                  "1 <rakim ft> <eski> <eski> <ICAO> <ad...>"
//   100          : kara pisti
//                  "100 <genislik m> <yuzey> <omuz> <puruzsuzluk> <orta isik>
//                   <kenar isik> <tabela>  <uc1>  <uc2>"
//                  her uc: "<no> <enlem> <boylam> <kayik esik m> <overrun m>
//                   <isaret> <yaklasma isigi> <TDZ> <REIL>"
//   99           : dosya sonu
// Diger tum satirlar (taksi yolu, isik, tabela, 1302 bilgi...) atlanir.
//
// Dosya sirasi (X-Plane'in kendi kurali, ayni ICAO icin once gelen kazanir):
//   1. Custom Scenery/scenery_packs.ini'deki SCENERY_PACK'ler, yukaridan asagi
//      (SCENERY_PACK_DISABLED atlanir), her birinin Earth nav data/apt.dat'i
//   2. Resources/default scenery/default apt dat/Earth nav data/apt.dat
//
// X-Plane'e bagimli DEGIL; arka plan thread'inde calisir.
#pragma once

#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

#include "data/AirportGeometry.h"

// Satir satir apt.dat ayristirici (dosyadan bagimsiz; testlerde metinle beslenir)
class AptDatParser
{
public:
    // pSeen: daha once alinmis ICAO'lar (onceki dosyalardan); bunlar atlanir
    // ve yeni alinanlar eklenir. pOut: bitmis havalimanlari buraya eklenir.
    AptDatParser(std::unordered_set<std::string>& pSeen, std::vector<AirportGeometry>& pOut);

    void ParseLine(const char* pLine, size_t pLength);
    void Finish();  // dosya bitti: yarim kalan havalimanini kapat

    int RunwayCount() const;  // bu ayristiricinin aldigi pist sayisi

    // Tek satir yardimcilari (testler icin acik)
    static bool ParseAirportHeader(const char* pLine, size_t pLength, std::string* pOutIcao,
                                   std::string* pOutName);
    static bool ParseRunway(const char* pLine, size_t pLength, Runway* pOut);

private:
    void CloseAirport();

    std::unordered_set<std::string>& mSeen;
    std::vector<AirportGeometry>& mOut;
    AirportGeometry mCurrent;
    bool mHasCurrent = false;
    bool mSkipCurrent = false;  // bu ICAO onceki bir dosyadan zaten alindi
    int mRunwayCount = 0;
};

class AptDatReader : public IAirportGeometrySource
{
public:
    const char* Name() const override;
    bool LoadAll(const std::string& pXPlaneDirectory, const std::atomic<bool>& pCancel,
                 std::vector<AirportGeometry>& pOutAirports, std::string& pOutReport) override;

    // scenery_packs.ini metninden okunacak apt.dat yollari (sirali). Dosyanin
    // var olup olmadigina bakmaz. Sonuna default apt.dat EKLENMEZ.
    static std::vector<std::string> AptFilesFromSceneryPacks(const std::string& pIniText,
                                                             const std::string& pXPlaneDirectory);

private:
    // Bir dosyayi parcalar halinde okuyup satirlari ayristiriciya verir
    static bool ReadFile(const std::string& pPath, AptDatParser& pParser, const std::atomic<bool>& pCancel);
};
