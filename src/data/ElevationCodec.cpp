// ElevationCodec.cpp
#include "data/ElevationCodec.h"

#include <cstdlib>
#include <cstring>

#include "LzmaDec.h"
#include "LzmaEnc.h"

namespace
{

const int kLzmaLevel = 4;                 // 0..9; 4 = orta (hiz oncelikli, 5 SDK varsayilani)
const unsigned kLzmaDictionary = 1u << 22;  // 4 MB: bir karonun butun verisi (2,9 MB) sigar
const size_t kPropsSize = LZMA_PROPS_SIZE;  // 5
const size_t kHeaderSize = kPropsSize + 4;  // ozellikler + ham boyut

void* LzmaAlloc(ISzAllocPtr /*pAllocator*/, size_t pSize)
{
    return std::malloc(pSize);
}

void LzmaFree(ISzAllocPtr /*pAllocator*/, void* pAddress)
{
    std::free(pAddress);
}

const ISzAlloc kAllocator = {LzmaAlloc, LzmaFree};

}  // namespace

namespace ElevationCodec
{

bool Compress(const std::vector<int16_t>& pValues, int pWidth, int pHeight, std::vector<uint8_t>* pOut)
{
    const size_t tCount = static_cast<size_t>(pWidth) * static_cast<size_t>(pHeight);
    if (pWidth <= 0 || pHeight <= 0 || pValues.size() != tCount)
    {
        return false;
    }

    // 1-2) Satir ici fark, bayt duzlemlerine ayrilmis: [dusuk baytlar ... | yuksek baytlar ...]
    std::vector<uint8_t> tPlanes(tCount * 2);
    for (int tRow = 0; tRow < pHeight; ++tRow)
    {
        uint16_t tPrevious = 0;
        for (int tColumn = 0; tColumn < pWidth; ++tColumn)
        {
            const size_t tIndex = static_cast<size_t>(tRow) * pWidth + tColumn;
            const uint16_t tValue = static_cast<uint16_t>(pValues[tIndex]);
            const uint16_t tDelta = static_cast<uint16_t>(tValue - tPrevious);
            tPlanes[tIndex] = static_cast<uint8_t>(tDelta & 0xFF);
            tPlanes[tCount + tIndex] = static_cast<uint8_t>(tDelta >> 8);
            tPrevious = tValue;
        }
    }

    // 3) LZMA
    CLzmaEncProps tProps;
    LzmaEncProps_Init(&tProps);
    tProps.level = kLzmaLevel;
    tProps.dictSize = kLzmaDictionary;
    tProps.numThreads = 1;
    // Sikismayan veri biraz buyuyebilir: ham boyut + %5 + 64 KB yeter (SDK onerisi)
    SizeT tDestinationSize = tPlanes.size() + tPlanes.size() / 20 + (1 << 16);
    pOut->assign(kHeaderSize + tDestinationSize, 0);
    SizeT tPropsSize = kPropsSize;
    const SRes tResult = LzmaEncode(pOut->data() + kHeaderSize, &tDestinationSize, tPlanes.data(), tPlanes.size(),
                                    &tProps, pOut->data(), &tPropsSize, 0, nullptr, &kAllocator, &kAllocator);
    if (tResult != SZ_OK || tPropsSize != kPropsSize)
    {
        pOut->clear();
        return false;
    }
    const uint32_t tRawSize = static_cast<uint32_t>(tPlanes.size());
    for (int tByte = 0; tByte < 4; ++tByte)
    {
        (*pOut)[kPropsSize + tByte] = static_cast<uint8_t>((tRawSize >> (8 * tByte)) & 0xFF);
    }
    pOut->resize(kHeaderSize + tDestinationSize);
    return true;
}

bool Decompress(const uint8_t* pData, size_t pSize, int pWidth, int pHeight, std::vector<int16_t>* pOut)
{
    const size_t tCount = static_cast<size_t>(pWidth) * static_cast<size_t>(pHeight);
    if (pWidth <= 0 || pHeight <= 0 || pSize < kHeaderSize)
    {
        return false;
    }
    uint32_t tRawSize = 0;
    for (int tByte = 0; tByte < 4; ++tByte)
    {
        tRawSize |= static_cast<uint32_t>(pData[kPropsSize + tByte]) << (8 * tByte);
    }
    if (tRawSize != tCount * 2)
    {
        return false;  // baska boyutta bir izgara: bozuk ya da eski dosya
    }

    std::vector<uint8_t> tPlanes(tRawSize);
    SizeT tDestinationSize = tRawSize;
    SizeT tSourceSize = pSize - kHeaderSize;
    ELzmaStatus tStatus = LZMA_STATUS_NOT_SPECIFIED;
    const SRes tResult = LzmaDecode(tPlanes.data(), &tDestinationSize, pData + kHeaderSize, &tSourceSize, pData,
                                    static_cast<unsigned>(kPropsSize), LZMA_FINISH_END, &tStatus, &kAllocator);
    // Bitis isareti yazilmadi: veri tam ham boyutta biter (LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK)
    if (tResult != SZ_OK || tDestinationSize != tRawSize)
    {
        return false;
    }

    // Farklari geri topla
    pOut->resize(tCount);
    for (int tRow = 0; tRow < pHeight; ++tRow)
    {
        uint16_t tValue = 0;
        for (int tColumn = 0; tColumn < pWidth; ++tColumn)
        {
            const size_t tIndex = static_cast<size_t>(tRow) * pWidth + tColumn;
            const uint16_t tDelta = static_cast<uint16_t>(tPlanes[tIndex] | (tPlanes[tCount + tIndex] << 8));
            tValue = static_cast<uint16_t>(tValue + tDelta);
            (*pOut)[tIndex] = static_cast<int16_t>(tValue);
        }
    }
    return true;
}

}  // namespace ElevationCodec
