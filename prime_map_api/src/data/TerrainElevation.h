// Bir 1x1 derece karonun DEM'i (1201 x 1201 post, karo kenarlari komsuyla PAYLASILIR)
// ve 3 x 3 komsulukla ornekleyici. Satir 0 = KUZEY saklanir. sea_level (256 x 256,
// hucre merkezli): verisi olmayan deniz hucresi -10, gollerde >= 0.
// Sampler: ortak kenar/kose noktalarinda VAR OLAN karolarin ortalamasi; yuklu olmayan
// karo icin merkezden aynali uzatma P(-k) = 2 P(0) - P(k).
#ifndef PRIME_MAP_SRC_DATA_TERRAINELEVATION_H
#define PRIME_MAP_SRC_DATA_TERRAINELEVATION_H

#include <cstdint>
#include <string>
#include <vector>

struct DsfRaster;

class ElevationGrid
{
public:
    static const int kLevelCount = 5;

    // DSF raster'larindan kurar (satir 0 = guney); post-centric degilse post izgarasina
    // cift dogrusal orneklenir. Basarisizsa (bos / cok kucuk) false.
    bool build(const DsfRaster& pElevation, const DsfRaster* pSeaLevel);

    bool isEmpty() const;
    int size(int pLevel) const;                           // kenardaki nokta sayisi
    float post(int pLevel, int pColumn, int pRow) const;  // satir 0 = kuzey
    double cellMeters(int pLevel) const;                  // nokta araligi (metre, enlem yonu)
    // nokta araligi piksel boyunu gecmeyen en kaba seviye
    int levelFor(double pMetersPerPixel) const;
    // 1201 noktali (XP11) izgara varsayimiyla seviye (izgara yuklenmeden once)
    static int nominalLevelFor(double pMetersPerPixel);

    // pFinestKept'ten ince seviyeleri birakir (~0,5 MB kalir; tam izgara ~5 MB).
    // Birakilan seviye bir daha okunamaz; save yapilamaz.
    void dropFineLevels(int pFinestKept);
    int finestLevel() const;
    bool hasLevel(int pLevel) const;

    bool hasSeaLevel() const;
    int seaLevelSize() const;
    float seaLevelCell(int pColumn, int pRow) const;  // satir 0 = kuzey, hucre merkezli

    // Onbellek dosyasi; anahtar zemin raster'iyla ayni (kaynak DSF'ler degisince gecersiz)
    bool save(const std::string& pPath, const std::string& pKey) const;
    bool load(const std::string& pPath, const std::string& pKey);

    size_t memoryBytes() const;

private:
    void buildPyramid();

    int mBaseSize = 0;
    int mFinestLevel = 0;                    // DropFineLevels sonrasi en ince seviye
    std::vector<int16_t> mBase;              // seviye 0, metre
    std::vector<std::vector<float>> mUpper;  // seviye 1.. (ortalamalar ondalikli)
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
    // pTiles[satir][sutun]; merkez [1][1] bos olmamali, komsular nullptr olabilir
    ElevationSampler(const ElevationGrid* const pTiles[3][3], int pLevel);

    int level() const;
    double cellMeters() const;

    // pU: 0..1 batidan doguya, pV: 0..1 kuzeyden guneye; yukseklik(metre) ve egim
    // (metre / karo birimi, u ve v yonunde).
    void sample(double pU, double pV, double* pHeight, double* pDhDu, double* pDhDv) const;

    // sea_level bikubik; merkez karonun sea_level'i yoksa false
    bool seaLevelAt(double pU, double pV, double* pValue) const;

private:
    double postAt(int pColumn, int pRow) const;
    double postFromTiles(int pColumn, int pRow, bool* pFound) const;
    double seaCellAt(int pColumn, int pRow) const;

    const ElevationGrid* mTiles[3][3];
    int mLevel = 0;
    int mSize = 0;  // bu seviyede kenardaki nokta sayisi
};
#endif  // PRIME_MAP_SRC_DATA_TERRAINELEVATION_H
