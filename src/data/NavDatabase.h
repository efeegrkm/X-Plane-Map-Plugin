// NavDatabase.h
// X-Plane'in nav verisinin (havalimani, VOR, DME, NDB, ILS, fix) bizdeki
// kopyasi + her tur icin ayri bir 1x1 derece izgara (GeoGrid).
// X-Plane'e bagimli DEGIL: kayitlari NavLoader doldurur, testler elle doldurur.
//
// Neden izgara: ~200 bin fix var. Her karede hepsine bakmak yerine sadece
// ekranda gorunen hucrelerin kayitlarina bakiyoruz.
#pragma once

#include <string>
#include <vector>

#include "GeoGrid.h"
#include "MapView.h"

enum class NavKind : unsigned char
{
    Airport,
    Vor,        // VOR, VOR/DME, VORTAC, TACAN
    Dme,        // tek basina DME (bir VOR/ILS ile eslesmeyen)
    Ndb,
    Ils,        // ILS localizer'i
    Localizer,  // tek basina localizer (LOC, LDA, SDF)
    Fix,
};

const int kNavKindCount = 7;

struct NavRecord
{
    NavKind mKind = NavKind::Fix;
    float mLat = 0.0f;
    float mLon = 0.0f;
    int mFrequency = 0;          // nav.dat kurali: NDB kHz (tam), digerleri MHz * 100
    float mCourseTrue = 0.0f;    // ILS/LOC: yaklasma yonu (gercek, derece)
    short mCourseMagnetic = -1;  // ILS/LOC: manyetik yon (bilinmiyorsa -1)
    bool mHasDme = false;        // VOR/ILS: ayni kimlikte bir DME ile eslesti
    bool mHidden = false;        // DME: bir VOR/ILS'e baglandi, ayrica cizilmez
    bool mMajor = false;         // havalimani: 4 harfli ICAO kodu (seyreltmede oncelikli)
    std::string mId;
    std::string mName;           // fix'lerde bos (yer tasarrufu)
};

// ILS/LOC yonu nav.dat 1150 bicimiyle "manyetik * 360 + gercek" olarak
// kodlanmis olabilir (ornek: 84112.655 = 233 * 360 + 232.655).
// pRaw 360'tan kucukse kodlanmamis kabul edilir (manyetik bilinmiyor: -1).
void DecodeLocalizerCourse(double pRaw, float* pOutTrue, short* pOutMagnetic);

// 4 harfli, rakamsiz kod: buyuk (ICAO) havalimani say. Rakamlilar (LT51,
// K1A2 ...) cogunlukla kucuk pist, heliport vb. (pist verisi gelince iyilesecek)
bool IsMajorAirportId(const std::string& pId);

// Bos veya sadece '-' olan kimlik ("----"): X-Plane'in yer tutucu kayitlari,
// gercek bir istasyon degil. Haritaya alinmaz.
bool IsPlaceholderId(const std::string& pId);

// TACAN istasyonlarinin "frekansi" SDK'dan kanal numarasi olarak gelir
// (NUW: 85 = kanal 85X, 113.80 MHz). Gercek VHF nav frekanslari 108.00 MHz
// ve ustu oldugu icin (nav.dat'ta >= 10800) daha kucuk deger kanal demektir.
bool IsTacanChannel(int pFrequency);

class NavDatabase
{
public:
    void Clear();

    // Yukleme sirasinda: kaydi ekler (izgara Build'e kadar kurulmaz)
    void Add(const NavRecord& pRecord);

    // Tum kayitlar eklenince bir kez: DME'leri VOR/ILS ile eslestirir,
    // izgarayi kurar. Bundan sonra IsReady() true olur.
    void Build();

    bool IsReady() const;
    int Count() const;
    int CountOf(NavKind pKind) const;  // izgaradaki (gizli olmayan) kayit sayisi
    const NavRecord& At(int pIndex) const;

    // pBounds'un degdigi hucrelerdeki pKind kayitlarinin indekslerini pOut'a
    // EKLER (hucre hassasiyetinde: sinirin biraz disindakiler de gelebilir).
    void Query(NavKind pKind, const GeoBounds& pBounds, std::vector<int>& pOut) const;

private:
    void PairDmes();
    void BuildIndex();

    std::vector<NavRecord> mRecords;
    GeoGrid mGrids[kNavKindCount];  // tur basina bir izgara
    bool mReady = false;
};
