// AirportGeometry.h
// Havalimani OZETI: her zaman bellekte duran kisim (kimlik, ad, tur, pistler,
// kapsadigi alan ve apt.dat dosyasindaki yeri). X-Plane'e bagimli DEGIL.
//
// Iki seviyeli veri:
//   1. Ozet (bu dosya): acilista tum apt.dat dosyalari bir kez taranarak
//      ~37 bin havalimani icin cikarilir (data/AptDatIndexer). Pist katmani,
//      ILS eslestirme ve "hangi havalimanlari ekranda" sorusu bununla calisir.
//   2. Ayrinti (data/AirportDetail.h): yuzeyler, cizgiler, tabelalar, isiklar...
//      Sadece ekrandaki havalimanlari icin, dosyadaki yerine (mLocation)
//      dogrudan atlanip arka planda okunur (data/AirportDetailStore).
#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "Geo.h"
#include "MapView.h"

// Pist yuzeyi, cizim rengi icin gruplanmis (apt.dat yuzey kodlari parantezde)
enum class RunwaySurface : unsigned char
{
    Paved,        // asfalt (1), beton (2); XP12'nin asfalt/beton turleri (20-38, 50-57)
    Unpaved,      // cim (3), toprak (4), cakil (5), kuru gol yatagi (12)
    Water,        // su (13) ve su pistleri (101)
    Snow,         // kar / buz (14)
    Transparent,  // saydam (15): sadece isaretler var, yuzey cizilmez
};

// Satir kodu 1 / 16 / 17
enum class AirportType : unsigned char
{
    Land,
    Seaplane,
    Heliport,
};

struct RunwayEnd
{
    std::string mName;                  // pist numarasi: "03R", "16", "H1"
    LatLon mPosition = {0.0, 0.0};      // pist ucunun orta noktasi (kaydirilmis esik DAHIL degil)
    double mDisplacedThresholdM = 0.0;  // kaydirilmis esik: uctan iceri, metre
    double mOverrunM = 0.0;             // blast pad / stopway: uctan disari, metre
    int mMarkings = 0;                  // pist isareti kodu (0 yok, 1 gorsel, 2 hassas olmayan, 3 hassas ...)
    int mApproachLights = 0;            // yaklasma isigi kodu (0 yok, 1 ALSF-I ... 12 RAIL)
    bool mTouchdownLights = false;
    int mReil = 0;                      // 0 yok, 1 her yone, 2 tek yone
};

struct Runway
{
    double mWidthM = 0.0;
    RunwaySurface mSurface = RunwaySurface::Paved;
    int mSurfaceCode = 1;      // apt.dat yuzey kodu (ayrinti icin)
    int mShoulderCode = 0;     // 0 yok, 1 asfalt, 2 beton
    bool mCenterLights = false;
    int mEdgeLights = 0;       // 0 yok, 2 orta siddet
    bool mWater = false;       // 101 su pisti
    RunwayEnd mEnds[2];
};

// Havalimaninin hangi dosyada, hangi bayt araliginda oldugu
struct AptFileLocation
{
    int mFile = -1;         // AirportIndex::mFiles icindeki sira
    uint64_t mOffset = 0;   // baslik satirinin ilk bayti
    uint32_t mLength = 0;   // sonraki havalimanina (veya dosya sonuna) kadar bayt
};

struct AirportGeometry
{
    std::string mId;        // baslik satirindaki kimlik (X-Plane nav verisindekiyle ayni)
    std::string mIcaoCode;  // 1302 icao_code (varsa; X-Plane 10.45+ bunu "ICAO" olarak kullanir)
    std::string mName;
    AirportType mType = AirportType::Land;
    int mElevationFt = 0;
    bool mHasTower = false;  // kule frekansi (54 / 1054) var
    LatLon mReference = {0.0, 0.0};  // 1302 datum_lat/lon; yoksa kapsayan kutunun ortasi
    GeoBounds mBounds;       // havalimaninin tum koordinatlarini kapsayan kutu
    std::vector<Runway> mRunways;  // kara (100) ve su (101) pistleri
    int mPavementCount = 0;  // 110 sayisi (ayrinti var mi, ne kadar)
    int mNodeCount = 0;      // 111..116 sayisi
    AptFileLocation mLocation;
};

// Kaynagin ciktisi: okunan dosyalar ve havalimanlari
struct AirportIndex
{
    std::vector<std::string> mFiles;  // oncelik sirasiyla apt.dat yollari
    std::vector<AirportGeometry> mAirports;
};

class IAirportGeometrySource
{
public:
    virtual ~IAirportGeometrySource() = default;

    // Log'da gorunen ad
    virtual const char* Name() const = 0;

    // Tum havalimanlarinin ozetini cikarir. ARKA PLAN thread'inde cagrilir:
    // icinde XPLM fonksiyonu (Log dahil) cagrilmaz; log'a yazilacaklar
    // pOutReport'a eklenir. pCancel true olursa cabucak false donup cikar.
    // pXPlaneDirectory: "C:\X-Plane 11\" gibi, sonunda ayirici ile.
    virtual bool LoadAll(const std::string& pXPlaneDirectory, const std::atomic<bool>& pCancel,
                         AirportIndex& pOut, std::string& pOutReport) = 0;
};

// apt.dat yuzey kodunu gruba cevirir (bilinmeyen kod: Paved)
RunwaySurface SurfaceFromAptCode(int pCode);
