// SevenZip.cpp
#include "data/SevenZip.h"

#include <cstring>
#include <mutex>

extern "C"
{
#include "7z.h"
#include "7zAlloc.h"
#include "7zCrc.h"
}

namespace
{

const size_t kLookBufferSize = 1 << 16;

// LZMA SDK'ya bellekteki veriyi "dosya" gibi okutmak icin akis
struct MemoryInStream
{
    ISeekInStream mInterface;  // ilk uye olmali: SDK bu adresi verir, biz geri ceviririz
    const uint8_t* mData;
    size_t mSize;
    size_t mPosition;
};

MemoryInStream* FromInterface(ISeekInStreamPtr pStream)
{
    // SDK arayuzu const isaretci verir; okuma konumunu degistirebilmek icin const'u kaldiriyoruz
    return reinterpret_cast<MemoryInStream*>(const_cast<ISeekInStream*>(pStream));
}

SRes MemoryRead(ISeekInStreamPtr pStream, void* pBuffer, size_t* pSize)
{
    MemoryInStream* tStream = FromInterface(pStream);
    const size_t tLeft = tStream->mSize - tStream->mPosition;
    const size_t tCount = (*pSize < tLeft) ? *pSize : tLeft;
    std::memcpy(pBuffer, tStream->mData + tStream->mPosition, tCount);
    tStream->mPosition += tCount;
    *pSize = tCount;
    return SZ_OK;
}

SRes MemorySeek(ISeekInStreamPtr pStream, Int64* pPosition, ESzSeek pOrigin)
{
    MemoryInStream* tStream = FromInterface(pStream);
    Int64 tBase = 0;
    if (pOrigin == SZ_SEEK_CUR)
    {
        tBase = static_cast<Int64>(tStream->mPosition);
    }
    else if (pOrigin == SZ_SEEK_END)
    {
        tBase = static_cast<Int64>(tStream->mSize);
    }
    const Int64 tTarget = tBase + *pPosition;
    if (tTarget < 0 || tTarget > static_cast<Int64>(tStream->mSize))
    {
        return SZ_ERROR_PARAM;
    }
    tStream->mPosition = static_cast<size_t>(tTarget);
    *pPosition = tTarget;
    return SZ_OK;
}

const ISzAlloc kAllocMain = {SzAlloc, SzFree};
const ISzAlloc kAllocTemp = {SzAllocTemp, SzFreeTemp};

std::once_flag gCrcTableOnce;

}  // namespace

namespace SevenZip
{

bool IsArchive(const uint8_t* pData, size_t pSize)
{
    static const uint8_t kSignature[6] = {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C};
    return pSize >= 6 && std::memcmp(pData, kSignature, 6) == 0;
}

bool ExtractFirstFile(const std::vector<uint8_t>& pArchive, std::vector<uint8_t>* pOut, std::string* pError)
{
    std::call_once(gCrcTableOnce, []() { CrcGenerateTable(); });  // SDK: ilk kullanimdan once bir kez

    MemoryInStream tMemory;
    tMemory.mInterface.Read = MemoryRead;
    tMemory.mInterface.Seek = MemorySeek;
    tMemory.mData = pArchive.data();
    tMemory.mSize = pArchive.size();
    tMemory.mPosition = 0;

    std::vector<Byte> tLookBuffer(kLookBufferSize);
    CLookToRead2 tLook;
    LookToRead2_CreateVTable(&tLook, False);
    tLook.buf = tLookBuffer.data();
    tLook.bufSize = tLookBuffer.size();
    tLook.realStream = &tMemory.mInterface;
    LookToRead2_INIT(&tLook);

    CSzArEx tArchive;
    SzArEx_Init(&tArchive);
    SRes tResult = SzArEx_Open(&tArchive, &tLook.vt, &kAllocMain, &kAllocTemp);
    if (tResult != SZ_OK)
    {
        SzArEx_Free(&tArchive, &kAllocMain);
        *pError = "not a readable 7z archive (error " + std::to_string(tResult) + ")";
        return false;
    }

    bool tDone = false;
    for (UInt32 tFile = 0; tFile < tArchive.NumFiles && !tDone; ++tFile)
    {
        if (SzArEx_IsDir(&tArchive, tFile))
        {
            continue;
        }
        UInt32 tBlockIndex = 0xFFFFFFFF;
        Byte* tBuffer = nullptr;
        size_t tBufferSize = 0;
        size_t tOffset = 0;
        size_t tProcessed = 0;
        tResult = SzArEx_Extract(&tArchive, &tLook.vt, tFile, &tBlockIndex, &tBuffer, &tBufferSize, &tOffset,
                                 &tProcessed, &kAllocMain, &kAllocTemp);
        if (tResult == SZ_OK)
        {
            pOut->assign(tBuffer + tOffset, tBuffer + tOffset + tProcessed);
            tDone = true;
        }
        else
        {
            *pError = "7z extract failed (error " + std::to_string(tResult) + ")";
        }
        ISzAlloc_Free(&kAllocMain, tBuffer);
        break;
    }
    SzArEx_Free(&tArchive, &kAllocMain);
    if (!tDone && pError->empty())
    {
        *pError = "7z archive contains no file";
    }
    return tDone;
}

}  // namespace SevenZip
