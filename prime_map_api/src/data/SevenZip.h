// 7z arsivinden dosya cikaran LZMA SDK sarmalayicisi.
#ifndef PRIME_MAP_SRC_DATA_SEVENZIP_H
#define PRIME_MAP_SRC_DATA_SEVENZIP_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace SevenZip
{
bool isArchive(const uint8_t* pData, size_t pSize);

bool extractFirstFile(const std::vector<uint8_t>& pArchive, std::vector<uint8_t>* pOut, std::string* pError);
}
#endif
