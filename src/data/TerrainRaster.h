// TerrainRaster.h
// Bir 1x1 derece DSF karosunun haritaya cizilecek zemini:
//   - sinif raster'i: her piksel bir TerrainClass (su, sehir, orman ...)
//   - kabartma raster'i: DEM yuksekliginden golge (0..255)
//   - rakim raster'i: DEM'in kucultulmus hali (metre, int16; VFR renkleri icin)
// Rasterlar enlem/boylam izgarasinda: satir 0 = KUZEY kenari, sutun 0 = BATI.
// X-Plane'e bagimli DEGIL; arka plan thread'inde uretilir, diske onbelleklenir.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "data/TerrainClasses.h"

struct DsfRaster;
class TerrainTriangles;

class TerrainRaster
{
public:
    static const int kSize = 4096;       // sinif raster'i kenari: 1 derece / 4096 ~ 27 m enlem
    static const int kShadeSize = 1024;  // kabartma raster'i kenari (golge yumusak degisir)
    static const int kElevationSize = 512;  // rakim raster'i kenari (~200 m; renk icin yeterli)

    // Karoyu bosaltir (her piksel None); pWest/pSouth: karonun guney-bati kosesi
    void Reset(int pWest, int pSouth);

    int West() const;
    int South() const;

    // Ucgeni boyar: merkezi ucgenin icinde kalan pikseller pClass olur
    void FillTriangle(const double pLon[3], const double pLat[3], TerrainClass pClass);

    // DEM'den (satir 0 = guney) kabartma golgesini ve rakim raster'ini hesaplar.
    // DEM yoksa cagrilmaz: ikisi de bos kalir, zemin golgesiz cizilir.
    void ComputeShading(const DsfRaster& pElevation);
    bool HasShading() const;
    bool HasElevation() const;
    double ElevationAt(double pU, double pV) const;  // metre; 0..1 normalize koordinat, cift dogrusal

    TerrainClass ClassAt(int pX, int pY) const;
    int CountOf(TerrainClass pClass) const;  // teshis ve testler icin

    // Bukulmus sinif: kendi sinifi karaysa ve kaydirilmis noktadaki de karaysa o, yoksa kendisi.
    // pOffsetCells: kayma, karo raster hucre cinsinden (+x dogu, +y guney). Karo disina bukme yok.
    TerrainClass WarpedClassAt(int pX, int pY, double pOffsetCellsX, double pOffsetCellsY) const;

    // Raster'in [pX0, pX1) x [pY0, pY1) parcasindan pOutSize x pOutSize RGBA
    // goruntu uretir (kucultmede renkler ortalanir; su olmayan yerde golge uygulanir).
    // pStyle: Classes = paletle, Elevation = rakim rampasi, LandWater = iki renk.
    void BuildImage(int pX0, int pY0, int pX1, int pY1, int pOutSize, const TerrainPalette& pPalette,
                    const TerrainImageStyle& pStyle, std::vector<uint8_t>* pRgba) const;

    // Yakin zoom: karo pChunksPerSide x pChunksPerSide parcaya bolunur (32 veya 128;
    // TerrainTriangles::kChunksPerSide'in kati). pChunk. parcayi (satir 0 = kuzey)
    // raster'dan degil DOGRUDAN DSF ucgenlerinden pOutSize x pOutSize boyar.
    // Golge bu raster'dan bikubik (yumusak), rakim cift dogrusal okunur.
    //
    // Scenery stilinde kara renkleri [blend] meters mesafesinde yumusatilir
    // (LandColorField); su ve kiyi keskin kalir, golge sonra eklenir. Doku pay ile
    // uretilip kirpilir, komsu dokularla dikis olmaz.
    void BuildDetailImage(const TerrainTriangles& pTriangles, int pChunksPerSide, int pChunk, int pOutSize,
                          const TerrainPalette& pPalette, const TerrainImageStyle& pStyle,
                          std::vector<uint8_t>* pRgba) const;

    // Saydam (alfa 0) piksellerin rengini en yakin opak komsudan doldurur (alfa
    // 0 kalir). Dogrusal suzme / mipmap saydam pikselin siyah rengini kenara
    // karistirip koyu sacak yapmasin diye. Tamamen saydam goruntuye dokunmaz.
    static void BleedTransparent(int pSize, std::vector<uint8_t>* pRgba);

    // Onbellek dosyasi. pKey, raster'in hangi DSF'lerden uretildigini tanimlar;
    // Load, dosyadaki anahtar pKey ile ayni degilse false doner (yeniden uretilmeli).
    bool Save(const std::string& pPath, const std::string& pKey) const;
    bool Load(const std::string& pPath, const std::string& pKey);

private:
    double ShadeAt(double pU, double pV) const;        // 0..1 normalize koordinat, cift dogrusal
    double ShadeAtSmooth(double pU, double pV) const;  // ayni, bikubik (Catmull-Rom): yakinda elmas deseni yok

    // Bir goruntu pikselinin son rengi. pR/pG/pB: sinif renklerinin toplami
    // (pCovered ornek), pLand: kara ornegi sayisi, pSamples: toplam ornek,
    // pU/pV: pikselin karodaki yeri (0..1, v = 0 kuzey).
    // pSmoothShade: golge bikubik okunur (yakin zoom parcalari)
    void FinishPixel(double pR, double pG, double pB, int pCovered, int pLand, int pSamples, double pU, double pV,
                     const TerrainPalette& pPalette, const TerrainImageStyle& pStyle, bool pSmoothShade,
                     uint8_t* pPixel) const;

    int mWest = 0;
    int mSouth = 0;
    std::vector<uint8_t> mClasses;  // kSize * kSize
    std::vector<uint8_t> mShade;    // kShadeSize * kShadeSize (bossa golge yok)
    std::vector<int16_t> mElevation;  // kElevationSize^2, metre, satir 0 = kuzey (bossa yok)
};
