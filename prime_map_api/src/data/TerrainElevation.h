// Bir 1x1 derece karonun yukseklik verisi (DSF "elevation" DEM'i) ve komsu
// karolarla birlikte okuyan ornekleyici. X-Plane'e bagimli DEGIL.
// DEM: 1201 x 1201 post izgarasi (post-centric); karo kenar satirlari komsuyla PAYLASILIR.
// Satir 0 = KUZEY olarak saklanir (DSF'te satir 0 = guney; okurken cevrilir).
// Piramit [1 2 1] x [1 2 1] / 16; kenarda filtre sadece kenar boyunca (karonun disina bakilmaz).
// sea_level (256 x 256, hucre merkezli): verisi olmayan deniz hucresi -10, gollerde >= 0.
// Sampler: ortak kenar/kose noktalarinda VAR OLAN karolarin ortalamasi (komsu kenar
// noktalari ayni degil); yuklu olmayan karo icin merkezden aynali uzatma P(-k) = 2 P(0) - P(k).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct DsfRaster;

class ElevationGrid
{
public:
    static const int kLevelCount = 5;

    // DSF raster'larindan kurar (satir 0 = guney). pSeaLevel nullptr olabilir.
    // Izgara post-centric degilse ayni boyutta post izgarasina cift dogrusal
    // orneklenir. basarisizsa(bos / cok kucuk) false; izgara bos kalir.
    bool build(const DsfRaster& pElevation, const DsfRaster* pSeaLevel);

    bool isEmpty() const;
    int size(int pLevel) const;                           // kenardaki nokta sayisi
    float post(int pLevel, int pColumn, int pRow) const;  // satir 0 = kuzey; aralik disi cagrilmaz
    // Seviyenin nokta araligi (metre, enlem yonu)
    double cellMeters(int pLevel) const;
    // Doku pikselinin boyu (metre) icin seviye: nokta araligi piksel boyunu gecmeyen en kaba seviye
    // (ince seviyeleri birakilmis izgarada en ince eldeki seviyeden asagi inmez)
    int levelFor(double pMetersPerPixel) const;
    // 1201 noktali (XP11) izgara varsayimiyla seviye: izgara yuklenmeden once hangi seviyenin
    // gerekecegini bilmek icin (zemin iscisi, kaba rakim bellegi)
    static int nominalLevelFor(double pMetersPerPixel);

    // Bellek: pFinestKept'ten ince seviyeleri birakir (uzak zoom icin ~0,5 MB kalir; tam izgara ~5 MB).
    // Birakilan seviye bir daha okunamaz; Save yapilamaz.
    void dropFineLevels(int pFinestKept);
    int finestLevel() const;
    bool hasLevel(int pLevel) const;

    bool hasSeaLevel() const;
    int seaLevelSize() const;
    float seaLevelCell(int pColumn, int pRow) const;  // satir 0 = kuzey, hucre merkezli

    // Onbellek dosyasi (anahtar zemin raster'iyla ayni: kaynak DSF'ler degisince gecersiz).
    // Tam cozunurluk ve sea_level satir farki + LZMA ile yazilir; piramit yuklerken yeniden kurulur.
    bool save(const std::string& pPath, const std::string& pKey) const;
    bool load(const std::string& pPath, const std::string& pKey);

    size_t memoryBytes() const;

private:
    void buildPyramid();

    int mBaseSize = 0;
    int mFinestLevel = 0;                    // DropFineLevels sonrasi elde kalan en ince seviye
    std::vector<int16_t> mBase;              // seviye 0, metre (DSF'te zaten tam sayi)
    std::vector<std::vector<float>> mUpper;  // seviye 1.. (ortalamalar ondalikli kalir)
    int mSeaSize = 0;
    std::vector<int16_t> mSeaLevel;
};

// Bir goruntu icin 3 x 3 karonun rakimi (merkez [1][1]; satir 0 = kuzey); yuklu olmayan nullptr
struct ElevationTiles
{
    const ElevationGrid* mTiles[3][3] = {};
};

class ElevationSampler
{
public:
    // pTiles[satir][sutun], merkez [1][1] bos olmamali; komsular nullptr olabilir
    ElevationSampler(const ElevationGrid* const pTiles[3][3], int pLevel);

    int level() const;
    double cellMeters() const;

    // pU: 0..1 batidan doguya, pV: 0..1 kuzeyden guneye (merkez karoda; disari tasabilir).
    // yukseklik(metre) ve egim: metre / karo birimi (u ve v yonunde).
    void sample(double pU, double pV, double* pHeight, double* pDhDu, double* pDhDv) const;

    // sea_level, bikubik (komsu karolar dahil; komsu yoksa merkezin kenar hucresi). Merkezde yoksa false.
    bool seaLevelAt(double pU, double pV, double* pValue) const;

private:
    // Merkeze gore nokta indeksi (pColumn, pRow; satir 0 = kuzey); disari tasabilir
    double postAt(int pColumn, int pRow) const;
    double postFromTiles(int pColumn, int pRow, bool* pFound) const;
    double seaCellAt(int pColumn, int pRow) const;  // sea_level hucresi; komsu yoksa merkezin kenari

    const ElevationGrid* mTiles[3][3];
    int mLevel = 0;
    int mSize = 0;  // bu seviyede kenardaki nokta sayisi
};
