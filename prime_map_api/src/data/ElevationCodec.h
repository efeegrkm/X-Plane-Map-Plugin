// ElevationCodec.h
// 16 bitlik yukseklik izgarasini (DEM, metre) diske sikistirarak yazmak icin
// kodlayici. X-Plane'e bagimli DEGIL; zemin iscisinin thread'inde calisir.
//
// Neden: DSF'in tam cozunurluklu DEM'i (1201 x 1201 int16 = 2,9 MB/karo) zemin
// onbellegine girer. Arazi yuksekligi komsudan komsuya az degistigi icin once
// "fark" alinir, sonra LZMA ile sikistirilir (third_party/lzma, LzmaEnc):
//
//   1. satir icinde fark : d[0] = v[0], d[i] = v[i] - v[i-1]  (16 bit tasmali)
//   2. bayt duzlemleri   : once butun dusuk baytlar, sonra butun yuksek baytlar
//                          (farklarin yuksek bayti cogunlukla 0 / 255: LZMA
//                          tekrarlari daha iyi yakalar)
//   3. LZMA              : orta seviye (hiz oncelikli), tek thread
//
// Cikti bicimi: [5 bayt LZMA ozellikleri][uint32 ham boyut][LZMA verisi]
// Kayipsizdir: Decompress, Compress'e verilen degerleri birebir geri verir.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ElevationCodec
{

// pValues: pWidth * pHeight deger (satir satir). Basarisizsa false.
bool Compress(const std::vector<int16_t>& pValues, int pWidth, int pHeight, std::vector<uint8_t>* pOut);

// Compress ciktisini acar; boyut tutmazsa veya veri bozuksa false.
bool Decompress(const uint8_t* pData, size_t pSize, int pWidth, int pHeight, std::vector<int16_t>* pOut);

// Genel bayt dizisi icin sadece LZMA (fark / duzlem yok); vektor onbellegi (TerrainVectors) kullanir.
// Bicim ayni: [5 bayt ozellik][uint32 ham boyut][LZMA verisi]
bool PackBytes(const std::vector<uint8_t>& pBytes, std::vector<uint8_t>* pOut);
bool UnpackBytes(const uint8_t* pData, size_t pSize, std::vector<uint8_t>* pOut);

}  // namespace ElevationCodec
