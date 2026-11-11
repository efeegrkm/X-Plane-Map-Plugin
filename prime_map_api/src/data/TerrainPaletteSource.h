// TerrainPaletteSource.h
// Asama 3: arazi sinifi renklerini X-Plane'in kendi arazi dokularindan cikarir.
// X-Plane'e (SDK'ya) bagimli DEGIL; sadece dosya okur.
//
//   Resources/default scenery/1000 world terrain/terrain10/*.ter
//       BASE_TEX ../textures10/<...>.dds      (GERCEK VERI: DXT5, mipmap'li)
//   .ter dosya adi -> sinif (ClassifyTerrain: coni_*, grass_*, urban_* ...)
//   dokunun kucuk bir mip seviyesi (<= 64 piksel) cozulur, ortalama renk alinir
//   sinif rengi = o sinifin butun dokularinin ortalamasi (kullanici karari: sinif basina ortalama)
//
// Bolgesel kopyalar (terrain10_NAW, _SEA ...) kullanilmaz: global terrain10 seti her yerde
// gecerli ortalamayi verir. Su sinifi hesaplanmaz (dokusu harita icin anlamsiz).
// Sonuc onbellege yazilir; anahtar .ter sayisi + toplam boyut (X-Plane guncellenince yenilenir).
#pragma once

#include <array>
#include <string>

#include "data/TerrainClasses.h"

struct XPlaneClassColors
{
    std::array<TerrainColor, kTerrainClassCount> mColors = {};
    std::array<int, kTerrainClassCount> mTextureCounts = {};  // 0: bu sinif icin doku bulunamadi
};

// pCachePath: onbellek dosyasi (bos: onbelleksiz). pLog: kisa rapor satiri.
bool LoadXPlaneClassColors(const std::string& pXPlaneDirectory, const std::string& pCachePath,
                           XPlaneClassColors* pOut, std::string* pLog);

// Bir DDS dosyasinin ortalama rengi (kucuk mip seviyesinden). Desteklenen: DXT1, DXT3, DXT5, 32 bit BGRA.
bool AverageDdsColor(const std::string& pPath, TerrainColor* pOut);
