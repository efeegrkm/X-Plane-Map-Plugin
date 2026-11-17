// Bir 1x1 derece DSF karosunun sinif raster'i (her piksel bir TerrainClass) ve goruntu
// uretimi. Satir 0 = KUZEY, sutun 0 = BATI. Rakim burada saklanmaz (TerrainElevation).
#ifndef PRIME_MAP_SRC_DATA_TERRAINRASTER_H
#define PRIME_MAP_SRC_DATA_TERRAINRASTER_H

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
    static const int kSize = 4096;  // 1 derece / 4096 ~ 27 m enlem

    // TerrainImageKey::mAreas: DSF vektor alanlari zemine islensin mi
    static const int kAreaForest = 1;
    static const int kAreaBuiltUp = 2;

    void reset(int pWest, int pSouth);  // her piksel None

    int west() const;
    int south() const;

    // merkezi ucgenin icinde kalan pikseller pClass olur
    void fillTriangle(const double pLon[3], const double pLat[3], TerrainClass pClass);

    TerrainClass classAt(int pX, int pY) const;

    // Vektor alanlarini (orman, yerlesim) ayri sinif katmanina tarar: sadece karada, dislama haric.
    void fillAreas(const TerrainVectors& pVectors);
    int countOf(TerrainClass pClass) const;

    // Bukulmus sinif: karo raster hucre cinsinden kaydirma (+x dogu, +y guney); karo disina bukme yok.
    TerrainClass warpedClassAt(int pX, int pY, double pOffsetCellsX, double pOffsetCellsY) const;

    // [pX0,pX1) x [pY0,pY1) parcasindan pOutSize^2 RGBA uretir. pElevation: karo + komsularin rakimi
    // (nullptr: kabartmasiz). pAreaMask: kArea... bitleri.
    void buildImage(int pX0, int pY0, int pX1, int pY1, int pOutSize, const TerrainPalette& pPalette,
                    const TerrainImageStyle& pStyle, std::vector<uint8_t>* pRgba,
                    const ElevationTiles* pElevation = nullptr, int pAreaMask = 0) const;

    // Yakin zoom: pChunk. parca DOGRUDAN DSF ucgenlerinden boyanir (raster'dan degil).
    // Scenery stilinde kara renkleri [blend] ile yumusatilir; doku pay ile uretilip
    // kirpilir, komsu dokularla dikis olmaz.
    void buildDetailImage(const TerrainTriangles& pTriangles, int pChunksPerSide, int pChunk, int pOutSize,
                          const TerrainPalette& pPalette, const TerrainImageStyle& pStyle,
                          std::vector<uint8_t>* pRgba, const ElevationTiles* pElevation = nullptr,
                          const TerrainVectors* pVectors = nullptr, int pAreaMask = 0) const;

    // Alfa 0 piksellerin rengini en yakin opak komsudan doldurur: dogrusal suzme / mipmap
    // siyah rengi kenara karistirip koyu sacak yapmasin diye.
    static void bleedTransparent(int pSize, std::vector<uint8_t>* pRgba);

    // Onbellek dosyasi; Load, dosyadaki anahtar pKey ile ayni degilse false (yeniden uretilmeli)
    bool save(const std::string& pPath, const std::string& pKey) const;
    bool load(const std::string& pPath, const std::string& pKey);

    static bool isAreaAllowed(TerrainClass pArea, int pAreaMask);

private:
    TerrainClass areaClassAt(int pX, int pY, TerrainClass pOwn, int pAreaMask, bool* pBuiltUp) const;

    int mWest = 0;
    int mSouth = 0;
    std::vector<uint8_t> mClasses;  // kSize * kSize
    std::vector<uint8_t> mAreas;    // vektor alan sinifi (None = alan yok); bossa vektor yok
};
#endif  // PRIME_MAP_SRC_DATA_TERRAINRASTER_H
