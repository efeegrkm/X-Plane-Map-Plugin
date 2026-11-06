// TerrainRaster.h
// Bir 1x1 derece DSF karosunun haritaya cizilecek zemini: sinif raster'i (her
// piksel bir TerrainClass: su, sehir, orman ...) ve ondan goruntu uretimi.
// Rasterlar enlem/boylam izgarasinda: satir 0 = KUZEY kenari, sutun 0 = BATI.
// X-Plane'e bagimli DEGIL; arka plan thread'inde uretilir, diske onbelleklenir.
//
// 0.16: kabartma ve rakim artik burada saklanmaz. Rakim TerrainElevation'da
// (tam cozunurluk + piramit, ayri onbellek dosyasi); goruntu uretilirken her
// piksel icin oradan okunur (komsu karolar dahil). Goruntu adimlari:
//
//   1. sinif ornekleri (payli alan)   -> kara orani, kaplama, kara rengi
//   2. Scenery: kara renkleri yumusatma (LandColorField), sinir bukmesi (TerrainNoise)
//   3. su: kiyiya uzaklik (acik serit), kiyi cizgisi gucu (TerrainRelief)
//   4. her cikti pikseli: rakim + egim (ElevationSampler) -> kabartma carpani,
//      VFR bandi; su rengi + derinlik tonu (sea_level) + serit; kiyi cizgisi
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "data/TerrainClasses.h"

struct ElevationTiles;
class TerrainTriangles;
class TerrainVectors;

class TerrainRaster
{
public:
    static const int kSize = 4096;  // sinif raster'i kenari: 1 derece / 4096 ~ 27 m enlem

    // Goruntu alan maskesi (TerrainImageKey::mAreas): DSF vektor alanlari zemine islensin mi (Asama 2)
    static const int kAreaForest = 1;   // orman / cali / sulak / tarla poligonlari (.for)
    static const int kAreaBuiltUp = 2;  // yerlesim (.ags urban / ind / com, park)

    // Karoyu bosaltir (her piksel None); pWest/pSouth: karonun guney-bati kosesi
    void Reset(int pWest, int pSouth);

    int West() const;
    int South() const;

    // Ucgeni boyar: merkezi ucgenin icinde kalan pikseller pClass olur
    void FillTriangle(const double pLon[3], const double pLat[3], TerrainClass pClass);

    TerrainClass ClassAt(int pX, int pY) const;

    // DSF vektor alanlarini (orman, yerlesim) ayri bir sinif katmanina tarar: sadece karada, dislama
    // alanlari haric. Goruntude maske bitine gore kara sinifinin yerine gecer.
    void FillAreas(const TerrainVectors& pVectors);
    int CountOf(TerrainClass pClass) const;  // teshis ve testler icin

    // Bukulmus sinif: kendi sinifi karaysa ve kaydirilmis noktadaki de karaysa o, yoksa kendisi.
    // pOffsetCells: kayma, karo raster hucre cinsinden (+x dogu, +y guney). Karo disina bukme yok.
    TerrainClass WarpedClassAt(int pX, int pY, double pOffsetCellsX, double pOffsetCellsY) const;

    // Raster'in [pX0, pX1) x [pY0, pY1) parcasindan pOutSize x pOutSize RGBA goruntu uretir
    // (kucultmede renkler ortalanir). pStyle: Classes = paletle, Elevation = VFR bantlari / rampa,
    // LandWater = iki renk. pElevation: bu karo ve komsularinin rakimi (nullptr: kabartmasiz).
    // pAreaMask: kArea... bitleri (vektor alanlari).
    void BuildImage(int pX0, int pY0, int pX1, int pY1, int pOutSize, const TerrainPalette& pPalette,
                    const TerrainImageStyle& pStyle, std::vector<uint8_t>* pRgba,
                    const ElevationTiles* pElevation = nullptr, int pAreaMask = 0) const;

    // Yakin zoom: karo pChunksPerSide x pChunksPerSide parcaya bolunur (32 veya 128;
    // TerrainTriangles::kChunksPerSide'in kati). pChunk. parcayi (satir 0 = kuzey)
    // raster'dan degil DOGRUDAN DSF ucgenlerinden pOutSize x pOutSize boyar.
    //
    // Scenery stilinde kara renkleri [blend] meters mesafesinde yumusatilir
    // (LandColorField); su ve kiyi keskin kalir, golge sonra eklenir. Doku pay ile
    // uretilip kirpilir, komsu dokularla dikis olmaz.
    void BuildDetailImage(const TerrainTriangles& pTriangles, int pChunksPerSide, int pChunk, int pOutSize,
                          const TerrainPalette& pPalette, const TerrainImageStyle& pStyle,
                          std::vector<uint8_t>* pRgba, const ElevationTiles* pElevation = nullptr,
                          const TerrainVectors* pVectors = nullptr, int pAreaMask = 0) const;

    // Saydam (alfa 0) piksellerin rengini en yakin opak komsudan doldurur (alfa
    // 0 kalir). Dogrusal suzme / mipmap saydam pikselin siyah rengini kenara
    // karistirip koyu sacak yapmasin diye. Tamamen saydam goruntuye dokunmaz.
    static void BleedTransparent(int pSize, std::vector<uint8_t>* pRgba);

    // Onbellek dosyasi. pKey, raster'in hangi DSF'lerden uretildigini tanimlar;
    // Load, dosyadaki anahtar pKey ile ayni degilse false doner (yeniden uretilmeli).
    bool Save(const std::string& pPath, const std::string& pKey) const;
    bool Load(const std::string& pPath, const std::string& pKey);

    // Alan sinifi maskeye gore izinli mi (orman ailesi: kAreaForest; yerlesim / park: kAreaBuiltUp)
    static bool IsAreaAllowed(TerrainClass pArea, int pAreaMask);

private:
    // Kara pikselinin sinifi, varsa ve maske izin veriyorsa vektor alaninin sinifi; pBuiltUp: yerlesim mi
    TerrainClass AreaClassAt(int pX, int pY, TerrainClass pOwn, int pAreaMask, bool* pBuiltUp) const;

    int mWest = 0;
    int mSouth = 0;
    std::vector<uint8_t> mClasses;  // kSize * kSize
    std::vector<uint8_t> mAreas;    // kSize * kSize, vektor alan sinifi (None = alan yok); bossa vektor yok
};
