// Bir 1x1 derece DSF karosunun zemini: sinif raster'i (her piksel bir TerrainClass)
// ve ondan goruntu uretimi. Satir 0 = KUZEY kenari, sutun 0 = BATI.
// X-Plane'e bagimli DEGIL; arka plan thread'inde uretilir, diske onbelleklenir.
// Rakim burada saklanmaz (TerrainElevation); goruntude karo + komsularindan okunur.
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

    // Goruntu alan maskesi (TerrainImageKey::mAreas): DSF vektor alanlari zemine islensin mi
    static const int kAreaForest = 1;   // orman / cali / sulak / tarla poligonlari (.for)
    static const int kAreaBuiltUp = 2;  // yerlesim (.ags urban / ind / com, park)

    // Karoyu bosaltir (her piksel None); pWest/pSouth: karonun guney-bati kosesi
    void reset(int pWest, int pSouth);

    int west() const;
    int south() const;

    // Ucgeni boyar: merkezi ucgenin icinde kalan pikseller pClass olur
    void fillTriangle(const double pLon[3], const double pLat[3], TerrainClass pClass);

    TerrainClass classAt(int pX, int pY) const;

    // DSF vektor alanlarini (orman, yerlesim) ayri bir sinif katmanina tarar: sadece karada, dislama
    // alanlari haric. Goruntude maske bitine gore kara sinifinin yerine gecer.
    void fillAreas(const TerrainVectors& pVectors);
    int countOf(TerrainClass pClass) const;  // teshis ve testler icin

    // Bukulmus sinif: kendi sinifi karaysa ve kaydirilmis noktadaki de karaysa o, yoksa kendisi.
    // pOffsetCells: kayma, karo raster hucre cinsinden (+x dogu, +y guney). Karo disina bukme yok.
    TerrainClass warpedClassAt(int pX, int pY, double pOffsetCellsX, double pOffsetCellsY) const;

    // Raster'in [pX0, pX1) x [pY0, pY1) parcasindan pOutSize x pOutSize RGBA goruntu uretir
    // (kucultmede renkler ortalanir). pStyle: Classes = paletle, Elevation = VFR bantlari / rampa,
    // LandWater = iki renk. pElevation: bu karo ve komsularinin rakimi (nullptr: kabartmasiz).
    // pAreaMask: kArea... bitleri (vektor alanlari).
    void buildImage(int pX0, int pY0, int pX1, int pY1, int pOutSize, const TerrainPalette& pPalette,
                    const TerrainImageStyle& pStyle, std::vector<uint8_t>* pRgba,
                    const ElevationTiles* pElevation = nullptr, int pAreaMask = 0) const;

    // Yakin zoom: karo pChunksPerSide x pChunksPerSide parcaya bolunur (32 veya 128;
    // TerrainTriangles::kChunksPerSide'in kati). pChunk. parcayi (satir 0 = kuzey)
    // raster'dan degil DOGRUDAN DSF ucgenlerinden pOutSize x pOutSize boyar.
    //
    // Scenery stilinde kara renkleri [blend] meters mesafesinde yumusatilir
    // (LandColorField); su ve kiyi keskin kalir, golge sonra eklenir. Doku pay ile
    // uretilip kirpilir, komsu dokularla dikis olmaz.
    void buildDetailImage(const TerrainTriangles& pTriangles, int pChunksPerSide, int pChunk, int pOutSize,
                          const TerrainPalette& pPalette, const TerrainImageStyle& pStyle,
                          std::vector<uint8_t>* pRgba, const ElevationTiles* pElevation = nullptr,
                          const TerrainVectors* pVectors = nullptr, int pAreaMask = 0) const;

    // saydam(alfa 0) piksellerin rengini en yakin opak komsudan doldurur (alfa
    // 0 kalir). Dogrusal suzme / mipmap saydam pikselin siyah rengini kenara
    // karistirip koyu sacak yapmasin diye. Tamamen saydam goruntuye dokunmaz.
    static void bleedTransparent(int pSize, std::vector<uint8_t>* pRgba);

    // Onbellek dosyasi. pKey, raster'in hangi DSF'lerden uretildigini tanimlar;
    // Load, dosyadaki anahtar pKey ile ayni degilse false doner (yeniden uretilmeli).
    bool save(const std::string& pPath, const std::string& pKey) const;
    bool load(const std::string& pPath, const std::string& pKey);

    // Alan sinifi maskeye gore izinli mi (orman ailesi: kAreaForest; yerlesim / park: kAreaBuiltUp)
    static bool isAreaAllowed(TerrainClass pArea, int pAreaMask);

private:
    // Kara pikselinin sinifi, varsa ve maske izin veriyorsa vektor alaninin sinifi; pBuiltUp: yerlesim mi
    TerrainClass areaClassAt(int pX, int pY, TerrainClass pOwn, int pAreaMask, bool* pBuiltUp) const;

    int mWest = 0;
    int mSouth = 0;
    std::vector<uint8_t> mClasses;  // kSize * kSize
    std::vector<uint8_t> mAreas;    // kSize * kSize, vektor alan sinifi (None = alan yok); bossa vektor yok
};
