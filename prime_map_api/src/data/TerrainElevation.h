// TerrainElevation.h
// Bir 1x1 derece karonun yukseklik verisi (DSF "elevation" DEM'i) ve onu
// komsu karolarla birlikte okuyan ornekleyici. X-Plane'e bagimli DEGIL.
//
// --- ElevationGrid: tam cozunurluk + piramit ---------------------------------
// DSF'in DEM'i 1201 x 1201 "post" (nokta) izgarasidir: ilk ve son nokta tam karo
// kenarindadir (post-centric; komsu karolar kenar satirini PAYLASIR). Satir 0 =
// KUZEY olacak sekilde saklanir (DSF'te satir 0 = guney; okurken cevrilir).
//
// Uzak zoom'da bu izgarayi seyrek noktalardan okumak ortusme (aliasing) yapar.
// Bu yuzden bir kez bir piramit kurulur; her seviye bir oncekinin yarisidir:
//
//   seviye 0: 1201 nokta (~92 m)    seviye 1: 601 (~185 m)   seviye 2: 301 (~370 m)
//   seviye 3:  151 (~740 m)         seviye 4:  76 (~1,5 km)
//
// Kucultme filtresi: yeni nokta (i, j) = alttaki (2i-1 .. 2i+1, 2j-1 .. 2j+1)
// noktalarinin [1 2 1] x [1 2 1] / 16 agirlikli ortalamasi. (Duz 2x2 kutu
// ortalamasi izgarayi yarim hucre kaydirirdi; noktalar karo kenarina denk
// gelmez, karo sinirinda dikis olurdu.) Kenar noktalarinda filtre sadece kenar
// BOYUNCA uygulanir (karonun disina bakilmaz): her karo kenarini komsusundan
// bagimsiz ama ayni kuralla hesaplar.
//
// Ayrica DSF'in "sea_level" raster'i (256 x 256 hucre, hucre merkezli) saklanir.
// GERCEK VERI (2026-11-06, demo DSF'leri): denizde deniz tabaninin yuksekligi
// gibi davranir (Puget Sound en derin -242 m); verisi olmayan deniz hucresi -10,
// gollerde >= 0 (derinlik yok), karadaki degerlerin anlami yok (kullanilmaz).
//
// --- ElevationSampler: 3 x 3 karo ile okuma -----------------------------------
//        [0][0] KB   [0][1] K    [0][2] KD
//        [1][0] B    [1][1] MERKEZ [1][2] D       (satir 0 = kuzey)
//        [2][0] GB   [2][1] G    [2][2] GD
// Bikubik (Catmull-Rom) okuma 4 x 4 nokta ister; karo kenarinda bunlarin bir
// kismi komsu karodadir. Kural:
//   - Bir nokta birden cok karoda varsa (ortak kenar / kose) VAR OLAN butun
//     karolarin degerlerinin ortalamasi alinir. GERCEK VERI: komsu karolarin
//     ortak kenar noktalari ayni degil (ortalama 1-2 m, en cok ~30 m fark);
//     ortalama iki tarafin sinirda ayni yuksekligi gormesini saglar.
//   - Noktanin oldugu karo yuklu degilse merkezden aynali dogrusal uzatma:
//     P(-k) = 2 P(0) - P(k). Kenarda egim boylece tek tarafli fark olur.
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
    // orneklenir. Basarisizsa (bos / cok kucuk) false; izgara bos kalir.
    bool Build(const DsfRaster& pElevation, const DsfRaster* pSeaLevel);

    bool IsEmpty() const;
    int Size(int pLevel) const;                           // kenardaki nokta sayisi
    float Post(int pLevel, int pColumn, int pRow) const;  // satir 0 = kuzey; aralik disi cagrilmaz
    // Seviyenin nokta araligi (metre, enlem yonu)
    double CellMeters(int pLevel) const;
    // Doku pikselinin boyu (metre) icin seviye: nokta araligi piksel boyunu gecmeyen en kaba seviye
    // (ince seviyeleri birakilmis izgarada en ince eldeki seviyeden asagi inmez)
    int LevelFor(double pMetersPerPixel) const;
    // 1201 noktali (XP11) izgara varsayimiyla seviye: izgara yuklenmeden once hangi seviyenin
    // gerekecegini bilmek icin (zemin iscisi, kaba rakim bellegi)
    static int NominalLevelFor(double pMetersPerPixel);

    // Bellek: pFinestKept'ten ince seviyeleri birakir (uzak zoom icin ~0,5 MB kalir; tam izgara ~5 MB).
    // Birakilan seviye bir daha okunamaz; Save yapilamaz.
    void DropFineLevels(int pFinestKept);
    int FinestLevel() const;
    bool HasLevel(int pLevel) const;

    bool HasSeaLevel() const;
    int SeaLevelSize() const;
    float SeaLevelCell(int pColumn, int pRow) const;  // satir 0 = kuzey, hucre merkezli

    // Onbellek dosyasi (anahtar zemin raster'iyla ayni: kaynak DSF'ler degisince gecersiz).
    // Tam cozunurluk ve sea_level satir farki + LZMA ile yazilir; piramit yuklerken yeniden kurulur.
    bool Save(const std::string& pPath, const std::string& pKey) const;
    bool Load(const std::string& pPath, const std::string& pKey);

    size_t MemoryBytes() const;

private:
    void BuildPyramid();

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

    int Level() const;
    double CellMeters() const;

    // pU: 0..1 batidan doguya, pV: 0..1 kuzeyden guneye (merkez karoda; disari tasabilir).
    // Yukseklik (metre) ve egim: metre / karo birimi (u ve v yonunde).
    void Sample(double pU, double pV, double* pHeight, double* pDhDu, double* pDhDv) const;

    // sea_level, bikubik (komsu karolar dahil; komsu yoksa merkezin kenar hucresi). Merkezde yoksa false.
    bool SeaLevelAt(double pU, double pV, double* pValue) const;

private:
    // Merkeze gore nokta indeksi (pColumn, pRow; satir 0 = kuzey); disari tasabilir
    double PostAt(int pColumn, int pRow) const;
    double PostFromTiles(int pColumn, int pRow, bool* pFound) const;
    double SeaCellAt(int pColumn, int pRow) const;  // sea_level hucresi; komsu yoksa merkezin kenari

    const ElevationGrid* mTiles[3][3];
    int mLevel = 0;
    int mSize = 0;  // bu seviyede kenardaki nokta sayisi
};
