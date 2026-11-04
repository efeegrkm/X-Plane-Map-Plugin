// AirportDetail.h
// Bir havalimaninin AYRINTISI: apt.dat'taki satirlarin birebir karsiligi.
// X-Plane'e bagimli DEGIL. Sadece ekrandaki havalimanlari icin okunur
// (data/AirportDetailStore). Kaynak: X-Plane apt.dat 1100 spesifikasyonu
// (rev. 16.11.2021, "1130 Version") + gercek Global Airports dosyasinda
// gorulen farklar (asagida "GERCEK VERI" notlari).
//
// Dugum zincirleri (110 yuzey, 120 cizgi, 130 sinir):
//   111 dugum, 112 Bezier kontrollu dugum,
//   113 / 114 halkayi kapatan dugum (ilk dugume baglanir),
//   115 / 116 zinciri bitiren dugum (acik cizgi).
// Bezier kurali (spesifikasyonda yazmiyor; gercek veride olculdu: 2000
// halkada bu kuralla 78, ters kuralla 1719 halka kendini kesiyor):
//   kontrol noktasi dugumden CIKAN egriye aittir; dugume GELEN egri icin
//   kontrol noktasinin dugume gore aynasi (2*P - C) kullanilir.
#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "data/AirportGeometry.h"

struct AptNode
{
    LatLon mPosition;
    bool mHasControl = false;  // 112, 114, 116
    LatLon mControl;           // Bezier kontrol noktasi (bkz. kural yukarida)
    // Bu dugumden baslayan parcanin stili. GERCEK VERI: dugum sonunda 0-3
    // deger olabiliyor ("51 59 105" = iki boya + bir isik; "102" = sadece
    // isik). 1-99 boya cizgisi, 101-108 isik.
    std::array<int, 2> mLineTypes = {0, 0};  // 0 = yok
    int mLightType = 0;                      // 0 = yok
};

struct AptChain
{
    std::vector<AptNode> mNodes;
    bool mClosed = false;  // 113/114 ile bittiyse halka
};

struct AptPavement  // 110
{
    int mSurfaceCode = 1;
    double mSmoothness = 0.25;
    double mTextureHeadingDeg = 0.0;
    std::string mName;
    std::vector<AptChain> mRings;  // [0] dis sinir (saat yonu tersi), digerleri delik (saat yonu)
};

struct AptLinearFeature  // 120
{
    std::string mName;
    std::vector<AptChain> mChains;
};

struct AptBoundary  // 130
{
    std::string mName;
    std::vector<AptChain> mRings;  // [0] dis sinir, digerleri delik
};

struct AptHelipad  // 102
{
    std::string mName;
    LatLon mCenter;
    double mHeadingDeg = 0.0;
    double mLengthM = 0.0;
    double mWidthM = 0.0;
    int mSurfaceCode = 1;
    int mMarkings = 0;
    int mShoulderCode = 0;
    bool mEdgeLights = false;
};

struct AptViewpoint  // 14
{
    LatLon mPosition;
    double mHeightFt = 0.0;  // GERCEK VERI: spesifikasyon tamsayi diyor, dosyada "242.78" var
    std::string mName;
};

// 1300 (yeni) ve 15 (eski) kalkis / park noktalari; 1301 ust bilgisi dahil
struct AptStartLocation
{
    LatLon mPosition;
    double mHeadingDeg = 0.0;
    std::string mType;           // "gate", "hangar", "misc", "tie_down" (GERCEK VERI: alt cizgi); 15 icin bos
    std::string mAircraftTypes;  // "jets|turboprops" ...
    std::string mName;
    std::string mIcaoWidth;      // 1301: "A".."F"
    std::string mOperation;      // 1301: none, general_aviation, airline, cargo, military
    std::string mAirlines;       // 1301: "AAL SWA ..." (bos olabilir)
};

struct AptBeacon  // 18
{
    LatLon mPosition;
    int mType = 0;  // 0 yok, 1 sivil, 2 deniz ussu, 3 heliport, 4 askeri
    std::string mName;
};

struct AptWindsock  // 19
{
    LatLon mPosition;
    bool mLit = false;
    std::string mName;
};

struct AptSign  // 20
{
    LatLon mPosition;
    double mHeadingDeg = 0.0;
    int mSize = 1;      // 1-3 taksi tabelasi kucuk/orta/buyuk, 4-5 kalan mesafe tabelasi
    std::string mText;  // "{@L}A1{@R}31R-13L"
};

struct AptLightObject  // 21 (GERCEK VERI: sadece 6 alan; pist/ad alanlari yok)
{
    LatLon mPosition;
    int mType = 0;  // 1 VASI, 2 PAPI-4L, 3 PAPI-4R, 4 uzay mekigi PAPI, 5 uc renkli VASI, 6 wig-wag
    double mHeadingDeg = 0.0;
    double mGlideslopeDeg = 0.0;
};

enum class AptFrequencyKind
{
    Recorded,   // 50 / 1050: ATIS, AWOS, ASOS
    Unicom,     // 51 / 1051
    Clearance,  // 52 / 1052
    Ground,     // 53 / 1053
    Tower,      // 54 / 1054
    Approach,   // 55 / 1055
    Departure,  // 56 / 1056
};

struct AptFrequency
{
    AptFrequencyKind mKind = AptFrequencyKind::Recorded;
    int mKhz = 0;  // 50-56 satirlari 10 kHz cozunurlukte gelir; burada hep kHz
    std::string mName;
};

struct AptTaxiNode  // 1201
{
    LatLon mPosition;
    std::string mUsage;  // "dest", "init", "both", "junc"
    int mId = 0;
    std::string mName;
};

struct AptTaxiEdge  // 1202 (ucak) ve 1206 (yer araci)
{
    int mFrom = 0;
    int mTo = 0;
    bool mOneWay = false;
    bool mRunway = false;        // "runway" (gecis icin ATC izni gerekir)
    char mWidthClass = 0;        // GERCEK VERI: "taxiway_A".."taxiway_F" -> 'A'..'F'; 0 = belirtilmemis
    bool mGroundVehicles = false;  // 1206
    std::string mName;           // "A", "16L/34R"; bos olabilir
    std::vector<std::pair<std::string, std::string>> mActiveZones;  // 1204: (arrival/departure/ils, "16L,16C")
};

struct AptTruckParking  // 1400
{
    LatLon mPosition;
    double mHeadingDeg = 0.0;
    std::string mType;  // baggage_train, fuel_jets ...
    int mCount = 0;     // bagaj treni vagon sayisi
    std::string mName;
};

struct AptTruckDestination  // 1401
{
    LatLon mPosition;
    double mHeadingDeg = 0.0;
    std::string mTypes;  // "baggage_train|fuel_jets"
    std::string mName;
};

struct AirportDetail
{
    // Baslik ve pistler (ozetle ayni bilgi; ayrinti tek basina da kullanilabilsin)
    std::string mId;
    std::string mName;
    AirportType mType = AirportType::Land;
    int mElevationFt = 0;
    std::vector<Runway> mRunways;

    std::vector<AptHelipad> mHelipads;
    std::vector<AptPavement> mPavements;  // dosya sirasi = cizim sirasi (USTTEN alta)
    std::vector<AptLinearFeature> mLinearFeatures;
    std::vector<AptBoundary> mBoundaries;
    std::vector<AptViewpoint> mViewpoints;
    std::vector<AptStartLocation> mStartLocations;
    std::vector<AptBeacon> mBeacons;
    std::vector<AptWindsock> mWindsocks;
    std::vector<AptSign> mSigns;
    std::vector<AptLightObject> mLightObjects;
    std::vector<AptFrequency> mFrequencies;
    std::vector<AptTaxiNode> mTaxiNodes;
    std::vector<AptTaxiEdge> mTaxiEdges;
    std::vector<AptTruckParking> mTruckParkings;
    std::vector<AptTruckDestination> mTruckDestinations;
    std::vector<std::pair<std::string, std::string>> mMetadata;  // 1302 anahtar / deger

    int mTrafficFlowCount = 0;  // 1000 (ATC trafik akislari: okunur ama haritada kullanilmaz)
    int mSkippedLineCount = 0;  // taninmayan / bozuk satirlar (log icin)
    std::vector<std::string> mSkippedExamples;  // ilk birkac bozuk satir (teshis)
};
