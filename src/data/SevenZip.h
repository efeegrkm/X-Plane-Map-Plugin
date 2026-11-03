// SevenZip.h
// 7z arsivinden dosya cikarma (LZMA SDK uzerinde kucuk bir C++ sarmalayici).
// X-Plane 11, DSF sahne dosyalarini 7z ile sikistirarak dagitir; DSF'i
// okumadan once acmak gerekir. X-Plane'e bagimli DEGIL; arka plan
// thread'inde kullanilabilir.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace SevenZip
{

// Veri 7z arsivi mi? (ilk 6 bayt: '7' 'z' BC AF 27 1C)
bool IsArchive(const uint8_t* pData, size_t pSize);

// Bellekteki arsivin ilk (klasor olmayan) dosyasini pOut'a acar.
// Basarisizsa false ve pError'a neden.
bool ExtractFirstFile(const std::vector<uint8_t>& pArchive, std::vector<uint8_t>* pOut, std::string* pError);

}  // namespace SevenZip
