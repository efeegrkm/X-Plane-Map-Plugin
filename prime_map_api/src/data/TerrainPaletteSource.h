// Arazi sinifi renkleri X-Plane'in kendi arazi dokularindan cikarilir.
// X-Plane'e (SDK'ya) bagimli DEGIL; sadece dosya okur.
//
//   Resources/default scenery/1000 world terrain/terrain10/*.ter
//       BASE_TEX ../textures10/<...>.dds      (GERCEK VERI: DXT5, mipmap'li)
//   .ter dosya adi -> sinif (coni_*, grass_*, urban_* ...);
//   kucuk mip seviyesi (<= 64 piksel) cozulur, sinif rengi = sinifin
//   dokularinin ortalamasi.
//   Bolgesel kopyalar (terrain10_NAW, _SEA ...) kullanilmaz: global terrain10
//   seti her yerde gecerli. Su sinifi hesaplanmaz.
//   Onbellek anahtari: .ter sayisi + toplam boyut (X-Plane guncellenince yenilenir).
#ifndef PRIME_MAP_SRC_DATA_TERRAINPALETTESOURCE_H
#define PRIME_MAP_SRC_DATA_TERRAINPALETTESOURCE_H

#include <array>
#include <string>

#include "data/TerrainClasses.h"

struct XPlaneClassColors
{
    std::array<TerrainColor, kTerrainClassCount> mColors = {};
    std::array<int, kTerrainClassCount> mTextureCounts = {};  // 0: bu sinif icin doku bulunamadi
};

// pCachePath: onbellek dosyasi (bos: onbelleksiz). pLog: kisa rapor satiri.
bool loadXPlaneClassColors(const std::string& pXPlaneDirectory, const std::string& pCachePath,
                           XPlaneClassColors* pOut, std::string* pLog);

// Bir DDS dosyasinin ortalama rengi (kucuk mip seviyesinden). Desteklenen: DXT1, DXT3, DXT5, 32 bit BGRA.
bool averageDdsColor(const std::string& pPath, TerrainColor* pOut);
#endif  // PRIME_MAP_SRC_DATA_TERRAINPALETTESOURCE_H
