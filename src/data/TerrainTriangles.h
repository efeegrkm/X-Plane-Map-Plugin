// TerrainTriangles.h
// Yakin zoom icin bir 1x1 derece karonun temel arazi ucgenleri, hafif bicimde.
// X-Plane'e bagimli DEGIL; arka plan thread'inde doldurulur.
//
// Neden: sinif raster'i (TerrainRaster) derece basina 4096 piksel; enlem
// 47'de ~27 m x 18 m. Yaklasinca her raster pikseli ekranda bir blok olur.
// X-Plane'in haritasi ise ucgenleri vektor olarak cizdigi icin keskin kalir.
// Burada ucgenler saklanir ve yakin zoom'da ekrandaki kucuk parcalar
// (1/32 derece) dogrudan ucgenlerden, yuksek cozunurlukte boyanir
// (TerrainRaster::BuildDetailImage).
//
// Bellek: ucgen basina ~29 bayt (6 float + 1 sinif + parca indeksi);
// 1 milyon ucgenlik bir karo ~30 MB. TerrainStore en fazla 2 karo tutar.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "data/TerrainClasses.h"

class TerrainTriangles
{
public:
    static const int kChunksPerSide = 32;  // parca = 1/32 derece (~3,5 km enlem)

    // Karoyu bosaltir; pWest/pSouth: guney-bati kose
    void Reset(int pWest, int pSouth);

    int West() const;
    int South() const;

    // Ucgeni ekler ve kapsayan kutusunun degdigi her parcaya kaydeder
    void Add(const double pLon[3], const double pLat[3], TerrainClass pClass);

    size_t TriangleCount() const;
    size_t MemoryBytes() const;

    // pChunk: satir * kChunksPerSide + sutun (satir 0 = KUZEY, sutun 0 = BATI)
    const std::vector<uint32_t>& TrianglesIn(int pChunk) const;

    // pIndex. ucgenin kose koordinatlari: karonun bati/guney kosesine gore derece
    void Corners(uint32_t pIndex, double pLon[3], double pLat[3]) const;
    TerrainClass ClassOf(uint32_t pIndex) const;

private:
    int mWest = 0;
    int mSouth = 0;
    std::vector<float> mCoords;     // ucgen basina 6 float: (dlon, dlat) x 3
    std::vector<uint8_t> mClasses;  // ucgen basina sinif
    std::vector<std::vector<uint32_t>> mChunks;  // kChunksPerSide^2 liste
};
