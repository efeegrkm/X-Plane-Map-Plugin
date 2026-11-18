// Yukseklik izgarasini fark kodlamasi ve LZMA ile sikistirip acar.
#ifndef PRIME_MAP_SRC_DATA_ELEVATIONCODEC_H
#define PRIME_MAP_SRC_DATA_ELEVATIONCODEC_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ElevationCodec
{
bool compress(const std::vector<int16_t>& pValues, int pWidth, int pHeight, std::vector<uint8_t>* pOut);

bool decompress(const uint8_t* pData, size_t pSize, int pWidth, int pHeight, std::vector<int16_t>* pOut);

bool packBytes(const std::vector<uint8_t>& pBytes, std::vector<uint8_t>* pOut);
bool unpackBytes(const uint8_t* pData, size_t pSize, std::vector<uint8_t>* pOut);
}
#endif
