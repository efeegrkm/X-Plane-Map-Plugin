// AirwayDatabase.h
// Hava yollari: earth_awy.dat (X-Plane 11, 1100 bicimi). X-Plane'e bagimli DEGIL.
//
// Dosyanin gercek satiri (her satir bir segment):
//   ABKAL LT 11 ULSAB LT 11 N 1 195 285 G8-L614
//   <kimlik1> <bolge1> <tur1> <kimlik2> <bolge2> <tur2> <yon> <seviye> <taban FL> <tavan FL> <adlar>
//   tur  : 11 = fix, 2 = NDB, 3 = VOR (VHF)
//   yon  : N = iki yonlu, F = sadece 1 -> 2, B = sadece 2 -> 1
//   seviye: 1 = alcak (low), 2 = yuksek (high)
//   adlar: bu segmenti kullanan hava yollari, '-' ile birlesik
//
// Uc noktalarin koordinati dosyada YOK: NavDatabase'den (X-Plane'in nav
// verisi) kimlik + tur ile bulunur. Ayni kimlikte birden cok aday olabilir;
// birbirine en yakin aday cifti secilir.
//
// Okuma arka plan thread'inde yapilir (StartLoading / Poll). NavDatabase o
// sirada degismemeli (NavLoader bitmis olmali).
#pragma once

#include <string>
#include <vector>

#include "BackgroundJob.h"
#include "GeoGrid.h"
#include "Geo.h"

class NavDatabase;

struct AirwaySegment
{
    LatLon mFrom = {0.0, 0.0};
    LatLon mTo = {0.0, 0.0};
    bool mHigh = false;      // seviye 2: yuksek hava yolu
    char mDirection = 'N';   // 'N' iki yonlu, 'F' mFrom -> mTo, 'B' mTo -> mFrom
    int mBaseFl = 0;
    int mTopFl = 0;
    std::string mNames;      // "G8-L614"
};

// Dosyadaki bir satirin ham hali (uc noktalar henuz cozulmemis)
struct AirwayLine
{
    std::string mFromId;
    int mFromType = 0;
    std::string mToId;
    int mToType = 0;
    char mDirection = 'N';
    int mLevel = 1;
    int mBaseFl = 0;
    int mTopFl = 0;
    std::string mNames;
};

class AirwayDatabase
{
public:
    // Tek satiri ayristirir (baslik, bos ve "99" satirlari icin false)
    static bool ParseLine(const std::string& pLine, AirwayLine* pOut);

    // Dosyayi okuyup uc noktalari cozer (bekleten islem; testler ve arka plan isi icin).
    // pReport: log'a yazilacak ozet.
    bool LoadFile(const std::string& pPath, const NavDatabase& pNav, const std::atomic<bool>& pCancel,
                  std::string* pReport);

    // pPaths'i sirayla dener (ilk var olan okunur); arka planda calisir
    void StartLoading(const std::vector<std::string>& pPaths, const NavDatabase& pNav);
    bool Poll();  // ana thread: okuma bu cagrida bittiyse true (bir kez)

    bool IsLoading() const;
    bool IsReady() const;
    const std::string& LastReport() const;

    int Count() const;
    const AirwaySegment& At(int pIndex) const;

    // pBounds'a degebilecek segmentler (ayni segment birden cok kez gelebilir:
    // uzun segmentler birden cok hucreye konur; cagiran tekrarlari atmali)
    void Query(const GeoBounds& pBounds, std::vector<int>& pOut) const;

private:
    void BuildGrid();

    std::vector<AirwaySegment> mSegments;
    GeoGrid mGrid;
    BackgroundJob mJob;
    std::string mReport;
    bool mReady = false;
};
