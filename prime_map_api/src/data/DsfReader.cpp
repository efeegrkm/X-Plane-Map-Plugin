// DsfReader.cpp
#include "data/DsfReader.h"

#include <cstdlib>
#include <cstring>

namespace
{

constexpr uint32_t AtomId(char pA, char pB, char pC, char pD)
{
    return (static_cast<uint32_t>(pA) << 24) | (static_cast<uint32_t>(pB) << 16) | (static_cast<uint32_t>(pC) << 8) |
           static_cast<uint32_t>(pD);
}

const uint32_t kAtomHead = AtomId('H', 'E', 'A', 'D');
const uint32_t kAtomProp = AtomId('P', 'R', 'O', 'P');
const uint32_t kAtomDefn = AtomId('D', 'E', 'F', 'N');
const uint32_t kAtomTert = AtomId('T', 'E', 'R', 'T');
const uint32_t kAtomObjt = AtomId('O', 'B', 'J', 'T');
const uint32_t kAtomPoly = AtomId('P', 'O', 'L', 'Y');
const uint32_t kAtomNetw = AtomId('N', 'E', 'T', 'W');
const uint32_t kAtomGeod = AtomId('G', 'E', 'O', 'D');
const uint32_t kAtomPool = AtomId('P', 'O', 'O', 'L');
const uint32_t kAtomScal = AtomId('S', 'C', 'A', 'L');
const uint32_t kAtomPool32 = AtomId('P', 'O', '3', '2');
const uint32_t kAtomScal32 = AtomId('S', 'C', '3', '2');
const uint32_t kAtomCmds = AtomId('C', 'M', 'D', 'S');
const uint32_t kAtomDemn = AtomId('D', 'E', 'M', 'N');
const uint32_t kAtomDems = AtomId('D', 'E', 'M', 'S');
const uint32_t kAtomDemi = AtomId('D', 'E', 'M', 'I');
const uint32_t kAtomDemd = AtomId('D', 'E', 'M', 'D');

const size_t kHeaderSize = 12;  // "XPLNEDSF" + uint32 surum
const size_t kFooterSize = 16;  // MD5
const size_t kAtomHeaderSize = 8;

// Komut kimlikleri (bicim belgesindeki tablo)
enum Command : uint8_t
{
    kPoolSelect = 1,
    kJunctionOffset = 2,
    kSetDefinition8 = 3,
    kSetDefinition16 = 4,
    kSetDefinition32 = 5,
    kRoadSubtype8 = 6,
    kObject = 7,
    kObjectRange = 8,
    kNetworkChain = 9,
    kNetworkChainRange = 10,
    kNetworkChain32 = 11,
    kPolygon = 12,
    kPolygonRange = 13,
    kNestedPolygon = 14,
    kNestedPolygonRange = 15,
    kTerrainPatch = 16,
    kTerrainPatchFlags = 17,
    kTerrainPatchFlagsLod = 18,
    kTriangle = 23,
    kTriangleCrossPool = 24,
    kTriangleRange = 25,
    kTriangleStrip = 26,
    kTriangleStripCrossPool = 27,
    kTriangleStripRange = 28,
    kTriangleFan = 29,
    kTriangleFanCrossPool = 30,
    kTriangleFanRange = 31,
    kComment8 = 32,
    kComment16 = 33,
    kComment32 = 34,
};

// Sinir kontrollu kucuk-endian okuyucu. Tasma olursa mFailed olur ve 0 doner.
struct Cursor
{
    const uint8_t* mData;
    size_t mSize;
    size_t mPosition = 0;
    bool mFailed = false;

    bool Has(size_t pBytes)
    {
        if (mPosition + pBytes > mSize)
        {
            mFailed = true;
            return false;
        }
        return true;
    }

    uint8_t U8()
    {
        if (!Has(1))
        {
            return 0;
        }
        return mData[mPosition++];
    }

    uint16_t U16()
    {
        if (!Has(2))
        {
            return 0;
        }
        const uint16_t tValue = static_cast<uint16_t>(mData[mPosition] | (mData[mPosition + 1] << 8));
        mPosition += 2;
        return tValue;
    }

    uint32_t U32()
    {
        if (!Has(4))
        {
            return 0;
        }
        const uint32_t tValue = static_cast<uint32_t>(mData[mPosition]) |
                                (static_cast<uint32_t>(mData[mPosition + 1]) << 8) |
                                (static_cast<uint32_t>(mData[mPosition + 2]) << 16) |
                                (static_cast<uint32_t>(mData[mPosition + 3]) << 24);
        mPosition += 4;
        return tValue;
    }

    float F32()
    {
        const uint32_t tBits = U32();
        float tValue = 0.0f;
        std::memcpy(&tValue, &tBits, sizeof(tValue));
        return tValue;
    }

    void Skip(size_t pBytes)
    {
        if (Has(pBytes))
        {
            mPosition += pBytes;
        }
    }
};

// Atom-of-atoms icindeki alt atomlari gezer
template <typename tVisitor>
void ForEachAtom(const uint8_t* pData, size_t pSize, tVisitor pVisitor)
{
    size_t tPosition = 0;
    while (tPosition + kAtomHeaderSize <= pSize)
    {
        Cursor tHeader{pData + tPosition, pSize - tPosition};
        const uint32_t tId = tHeader.U32();
        const uint32_t tSize = tHeader.U32();
        if (tSize < kAtomHeaderSize || tPosition + tSize > pSize)
        {
            return;  // bozuk atom: geri kalani okunamaz
        }
        pVisitor(tId, pData + tPosition + kAtomHeaderSize, tSize - kAtomHeaderSize);
        tPosition += tSize;
    }
}

// Duzlemsel sayi atomunun bir duzlemini cozer (16 bit). pOut bos ise atlanir.
// Bir duzlemin degerlerini cozer: ham / fark / RLE / RLE + fark. TValue: uint16_t (POOL) veya uint32_t (PO32);
// kurallar ikisinde ayni, sadece deger genisligi farkli.
template <typename TValue>
TValue ReadValue(Cursor& pCursor)
{
    if constexpr (sizeof(TValue) == 2)
    {
        return pCursor.U16();
    }
    else
    {
        return pCursor.U32();
    }
}

template <typename TValue>
bool DecodePlane(Cursor& pCursor, int pCount, std::vector<TValue>* pOut)
{
    const uint8_t tEncoding = pCursor.U8();
    const bool tDifferenced = (tEncoding == 1 || tEncoding == 3);
    const bool tRunLength = (tEncoding == 2 || tEncoding == 3);
    if (tEncoding > 3)
    {
        return false;
    }
    std::vector<TValue> tValues;
    tValues.reserve(static_cast<size_t>(pCount));
    if (!tRunLength)
    {
        for (int tIndex = 0; tIndex < pCount; ++tIndex)
        {
            tValues.push_back(ReadValue<TValue>(pCursor));
        }
    }
    else
    {
        // Kontrol bayti: ust bit 1 ise bir deger (alt 7 bit) kez tekrar,
        // 0 ise alt 7 bit kadar deger oldugu gibi
        while (static_cast<int>(tValues.size()) < pCount && !pCursor.mFailed)
        {
            const uint8_t tControl = pCursor.U8();
            const int tRun = tControl & 0x7F;
            if (tControl & 0x80)
            {
                const TValue tValue = ReadValue<TValue>(pCursor);
                for (int tRepeat = 0; tRepeat < tRun; ++tRepeat)
                {
                    tValues.push_back(tValue);
                }
            }
            else
            {
                for (int tLiteral = 0; tLiteral < tRun; ++tLiteral)
                {
                    tValues.push_back(ReadValue<TValue>(pCursor));
                }
            }
        }
        tValues.resize(static_cast<size_t>(pCount));
    }
    if (tDifferenced)
    {
        // Her deger bir oncekine gore fark: toplam al (16 / 32 bit tasma beklenen davranis)
        TValue tSum = 0;
        for (TValue& tValue : tValues)
        {
            tSum = static_cast<TValue>(tSum + tValue);
            tValue = tSum;
        }
    }
    if (pOut)
    {
        *pOut = std::move(tValues);
    }
    return !pCursor.mFailed;
}

}  // namespace

bool DsfReader::Open(const std::vector<uint8_t>& pData, std::string* pError)
{
    mData = &pData;
    mInfo = DsfInfo();
    mPools.clear();
    mPools32.clear();
    mCommandsOffset = 0;
    mCommandsSize = 0;
    mRasterNames.clear();
    mRasters.clear();

    if (pData.size() < kHeaderSize + kFooterSize || std::memcmp(pData.data(), "XPLNEDSF", 8) != 0)
    {
        *pError = "not a DSF file (no XPLNEDSF header)";
        return false;
    }
    Cursor tVersion{pData.data() + 8, 4};
    if (tVersion.U32() != 1)
    {
        *pError = "unsupported DSF version";
        return false;
    }

    int tScaleIndex = 0;    // k. SCAL, k. POOL'a aittir
    int tScale32Index = 0;  // k. SC32, k. PO32'ye aittir
    bool tPoolFailed = false;
    const size_t tBodySize = pData.size() - kHeaderSize - kFooterSize;
    ForEachAtom(pData.data() + kHeaderSize, tBodySize,
                [&](uint32_t pId, const uint8_t* pAtom, size_t pAtomSize)
                {
                    if (pId == kAtomHead)
                    {
                        ForEachAtom(pAtom, pAtomSize,
                                    [&](uint32_t pSubId, const uint8_t* pSub, size_t pSubSize)
                                    {
                                        if (pSubId == kAtomProp)
                                        {
                                            ReadProperties(pSub, pSubSize);
                                        }
                                    });
                    }
                    else if (pId == kAtomDefn)
                    {
                        ForEachAtom(pAtom, pAtomSize,
                                    [&](uint32_t pSubId, const uint8_t* pSub, size_t pSubSize)
                                    {
                                        if (pSubId == kAtomTert)
                                        {
                                            mInfo.mTerrainNames = ReadStringTable(pSub, pSubSize);
                                        }
                                        else if (pSubId == kAtomObjt)
                                        {
                                            mInfo.mObjectNames = ReadStringTable(pSub, pSubSize);
                                        }
                                        else if (pSubId == kAtomPoly)
                                        {
                                            mInfo.mPolygonNames = ReadStringTable(pSub, pSubSize);
                                        }
                                        else if (pSubId == kAtomNetw)
                                        {
                                            mInfo.mNetworkNames = ReadStringTable(pSub, pSubSize);
                                        }
                                        else if (pSubId == kAtomDemn)
                                        {
                                            mRasterNames = ReadStringTable(pSub, pSubSize);
                                        }
                                    });
                    }
                    else if (pId == kAtomGeod)
                    {
                        ForEachAtom(pAtom, pAtomSize,
                                    [&](uint32_t pSubId, const uint8_t* pSub, size_t pSubSize)
                                    {
                                        if (pSubId == kAtomPool)
                                        {
                                            Pool tPool;
                                            tPoolFailed = tPoolFailed || !ReadPool(pSub, pSubSize, &tPool);
                                            mPools.push_back(std::move(tPool));
                                        }
                                        else if (pSubId == kAtomScal && tScaleIndex < static_cast<int>(mPools.size()))
                                        {
                                            ReadScale(pSub, pSubSize, &mPools[tScaleIndex]);
                                            ++tScaleIndex;
                                        }
                                        else if (pSubId == kAtomPool32)
                                        {
                                            Pool32 tPool;
                                            tPoolFailed = tPoolFailed || !ReadPool32(pSub, pSubSize, &tPool);
                                            mPools32.push_back(std::move(tPool));
                                        }
                                        else if (pSubId == kAtomScal32 &&
                                                 tScale32Index < static_cast<int>(mPools32.size()))
                                        {
                                            Pool32& tPool = mPools32[tScale32Index];
                                            Cursor tCursor{pSub, pSubSize};
                                            for (int tPlane = 0; tPlane < tPool.mPlaneCount && tCursor.Has(8); ++tPlane)
                                            {
                                                const double tScale = tCursor.F32();
                                                const double tOffset = tCursor.F32();
                                                if (tPlane < 2)
                                                {
                                                    tPool.mScale[tPlane] = tScale;
                                                    tPool.mOffset[tPlane] = tOffset;
                                                }
                                            }
                                            ++tScale32Index;
                                        }
                                    });
                    }
                    else if (pId == kAtomCmds)
                    {
                        mCommandsOffset = static_cast<size_t>(pAtom - pData.data());
                        mCommandsSize = pAtomSize;
                    }
                    else if (pId == kAtomDems)
                    {
                        // DEMI + DEMD ciftleri, DEMN adlariyla ayni sirada
                        ForEachAtom(pAtom, pAtomSize,
                                    [&](uint32_t pSubId, const uint8_t* pSub, size_t pSubSize)
                                    {
                                        const size_t tOffset = static_cast<size_t>(pSub - pData.data());
                                        if (pSubId == kAtomDemi)
                                        {
                                            RasterAtoms tRaster;
                                            tRaster.mInfoOffset = tOffset;
                                            tRaster.mInfoSize = pSubSize;
                                            mRasters.push_back(tRaster);
                                        }
                                        else if (pSubId == kAtomDemd && !mRasters.empty())
                                        {
                                            mRasters.back().mDataOffset = tOffset;
                                            mRasters.back().mDataSize = pSubSize;
                                        }
                                    });
                    }
                });
    if (tPoolFailed)
    {
        *pError = "corrupt point pool";
        return false;
    }
    return true;
}

const DsfInfo& DsfReader::Info() const
{
    return mInfo;
}

bool DsfReader::ReadRaster(const std::string& pName, DsfRaster* pOut) const
{
    size_t tLayer = 0;
    while (tLayer < mRasterNames.size() && mRasterNames[tLayer] != pName)
    {
        ++tLayer;
    }
    if (!mData || tLayer >= mRasterNames.size() || tLayer >= mRasters.size())
    {
        return false;
    }
    const RasterAtoms& tAtoms = mRasters[tLayer];
    Cursor tInfo{mData->data() + tAtoms.mInfoOffset, tAtoms.mInfoSize};
    tInfo.U8();  // surum
    const int tBytesPerPixel = tInfo.U8();
    const int tFlags = tInfo.U16();
    const int tWidth = static_cast<int>(tInfo.U32());
    const int tHeight = static_cast<int>(tInfo.U32());
    const double tScale = tInfo.F32();
    const double tOffset = tInfo.F32();
    const int tType = tFlags & 3;  // 0 float, 1 isaretli, 2 isaretsiz tamsayi
    const size_t tCount = static_cast<size_t>(tWidth) * static_cast<size_t>(tHeight);
    if (tInfo.mFailed || tWidth <= 0 || tHeight <= 0 || tType == 3 ||
        tAtoms.mDataSize < tCount * static_cast<size_t>(tBytesPerPixel) ||
        (tBytesPerPixel != 1 && tBytesPerPixel != 2 && tBytesPerPixel != 4) || (tType == 0 && tBytesPerPixel != 4))
    {
        return false;
    }
    Cursor tData{mData->data() + tAtoms.mDataOffset, tAtoms.mDataSize};
    pOut->mWidth = tWidth;
    pOut->mHeight = tHeight;
    pOut->mPostCentric = (tFlags & 4) != 0;
    pOut->mValues.resize(tCount);
    for (size_t tIndex = 0; tIndex < tCount; ++tIndex)
    {
        double tRaw = 0.0;
        if (tType == 0)
        {
            tRaw = tData.F32();
        }
        else if (tBytesPerPixel == 1)
        {
            const uint8_t tByte = tData.U8();
            tRaw = (tType == 1) ? static_cast<int8_t>(tByte) : tByte;
        }
        else if (tBytesPerPixel == 2)
        {
            const uint16_t tWord = tData.U16();
            tRaw = (tType == 1) ? static_cast<int16_t>(tWord) : tWord;
        }
        else
        {
            const uint32_t tLong = tData.U32();
            tRaw = (tType == 1) ? static_cast<double>(static_cast<int32_t>(tLong)) : static_cast<double>(tLong);
        }
        pOut->mValues[tIndex] = static_cast<float>(tRaw * tScale + tOffset);
    }
    return true;
}

const std::vector<std::string>& DsfReader::RasterNames() const
{
    return mRasterNames;
}

int DsfReader::PoolCount() const
{
    return static_cast<int>(mPools.size());
}

std::vector<std::string> DsfReader::ReadStringTable(const uint8_t* pData, size_t pSize)
{
    // Ardisik C dizgileri; her biri bir sifir bayt ile biter
    std::vector<std::string> tStrings;
    size_t tStart = 0;
    for (size_t tIndex = 0; tIndex < pSize; ++tIndex)
    {
        if (pData[tIndex] == 0)
        {
            tStrings.emplace_back(reinterpret_cast<const char*>(pData + tStart), tIndex - tStart);
            tStart = tIndex + 1;
        }
    }
    return tStrings;
}

void DsfReader::ReadProperties(const uint8_t* pData, size_t pSize)
{
    const std::vector<std::string> tStrings = ReadStringTable(pData, pSize);
    for (size_t tIndex = 0; tIndex + 1 < tStrings.size(); tIndex += 2)
    {
        const std::string& tName = tStrings[tIndex];
        const std::string& tValue = tStrings[tIndex + 1];
        mInfo.mProperties.emplace_back(tName, tValue);
        const double tNumber = std::atof(tValue.c_str());  // tam sayi / basit ondalik: yerel ayar sorun degil
        if (tName == "sim/west")
        {
            mInfo.mWest = tNumber;
        }
        else if (tName == "sim/east")
        {
            mInfo.mEast = tNumber;
        }
        else if (tName == "sim/south")
        {
            mInfo.mSouth = tNumber;
        }
        else if (tName == "sim/north")
        {
            mInfo.mNorth = tNumber;
        }
        else if (tName == "sim/overlay")
        {
            mInfo.mOverlay = (tValue == "1");
        }
    }
}

bool DsfReader::ReadPool(const uint8_t* pData, size_t pSize, Pool* pOut) const
{
    Cursor tCursor{pData, pSize};
    pOut->mCount = static_cast<int>(tCursor.U32());
    pOut->mPlaneCount = tCursor.U8();
    for (int tPlane = 0; tPlane < pOut->mPlaneCount; ++tPlane)
    {
        std::vector<uint16_t>* tTarget = (tPlane < 3) ? &pOut->mPlanes[tPlane] : nullptr;
        if (!DecodePlane<uint16_t>(tCursor, pOut->mCount, tTarget))
        {
            return false;
        }
    }
    return !tCursor.mFailed;
}

bool DsfReader::ReadPool32(const uint8_t* pData, size_t pSize, Pool32* pOut) const
{
    Cursor tCursor{pData, pSize};
    pOut->mCount = static_cast<int>(tCursor.U32());
    pOut->mPlaneCount = tCursor.U8();
    for (int tPlane = 0; tPlane < pOut->mPlaneCount; ++tPlane)
    {
        std::vector<uint32_t>* tTarget = (tPlane < 2) ? &pOut->mPlanes[tPlane] : nullptr;
        if (!DecodePlane<uint32_t>(tCursor, pOut->mCount, tTarget))
        {
            return false;
        }
    }
    return !tCursor.mFailed;
}

bool DsfReader::ReadScale(const uint8_t* pData, size_t pSize, Pool* pPool) const
{
    Cursor tCursor{pData, pSize};
    for (int tPlane = 0; tPlane < pPool->mPlaneCount && tCursor.Has(8); ++tPlane)
    {
        const double tScale = tCursor.F32();
        const double tOffset = tCursor.F32();
        if (tPlane < 3)
        {
            pPool->mScale[tPlane] = tScale;
            pPool->mOffset[tPlane] = tOffset;
        }
    }
    return !tCursor.mFailed;
}

double DsfReader::PlaneValue(const Pool& pPool, int pPlane, int pIndex) const
{
    // 16 bitlik deger 0..65535 araligindan [kaydirma, kaydirma + olcek] araligina
    // (DSFLib: deger / 65535 * olcek + kaydirma; olcek 0 ise ham deger)
    const double tRaw = pPool.mPlanes[pPlane][static_cast<size_t>(pIndex)];
    if (pPool.mScale[pPlane] == 0.0)
    {
        return tRaw;
    }
    return tRaw / 65535.0 * pPool.mScale[pPlane] + pPool.mOffset[pPlane];
}

bool DsfReader::ForEachTriangle(const std::function<void(const DsfTriangle&)>& pVisitor, std::string* pError) const
{
    if (!mData || mCommandsSize == 0)
    {
        return true;  // komut yok: arazi yok
    }
    Cursor tCursor{mData->data() + mCommandsOffset, mCommandsSize};
    int tPool = -1;
    int tDefinition = 0;
    int tPatchTerrain = -1;  // patch yoksa -1 (ucgenler yok sayilir)
    int tPatchFlags = 0;

    DsfTriangle tTriangle;
    int tCorner = 0;
    bool tBadIndex = false;

    // Bir kose noktasini ucgene ekler; 3 kose dolunca ziyaretciye verir
    auto tEmit = [&](int pPoolIndex, int pPointIndex)
    {
        if (pPoolIndex < 0 || pPoolIndex >= static_cast<int>(mPools.size()))
        {
            tBadIndex = true;
            return;
        }
        const Pool& tSource = mPools[pPoolIndex];
        if (pPointIndex < 0 || pPointIndex >= tSource.mCount || tSource.mPlaneCount < 3)
        {
            tBadIndex = true;
            return;
        }
        tTriangle.mPlaneCount = tSource.mPlaneCount;
        tTriangle.mLon[tCorner] = PlaneValue(tSource, 0, pPointIndex);
        tTriangle.mLat[tCorner] = PlaneValue(tSource, 1, pPointIndex);
        tTriangle.mElevation[tCorner] = PlaneValue(tSource, 2, pPointIndex);
        if (++tCorner == 3)
        {
            tCorner = 0;
            if (tPatchTerrain >= 0)
            {
                tTriangle.mTerrain = tPatchTerrain;
                tTriangle.mPatchFlags = tPatchFlags;
                pVisitor(tTriangle);
            }
        }
    };

    // Serit: 0-1-2, 2-1-3, 2-3-4 ... ; yelpaze: 0-1-2, 0-2-3 ...
    std::vector<std::pair<int, int>> tVertices;  // (havuz, nokta)
    auto tEmitStrip = [&]()
    {
        for (size_t tIndex = 2; tIndex < tVertices.size(); ++tIndex)
        {
            tEmit(tVertices[tIndex - 2].first, tVertices[tIndex - 2].second);
            tEmit(tVertices[tIndex - 1].first, tVertices[tIndex - 1].second);
            tEmit(tVertices[tIndex].first, tVertices[tIndex].second);
        }
    };
    auto tEmitFan = [&]()
    {
        for (size_t tIndex = 2; tIndex < tVertices.size(); ++tIndex)
        {
            tEmit(tVertices[0].first, tVertices[0].second);
            tEmit(tVertices[tIndex - 1].first, tVertices[tIndex - 1].second);
            tEmit(tVertices[tIndex].first, tVertices[tIndex].second);
        }
    };

    while (tCursor.mPosition < tCursor.mSize && !tCursor.mFailed && !tBadIndex)
    {
        const uint8_t tCommand = tCursor.U8();
        switch (tCommand)
        {
            case kPoolSelect:
                tPool = tCursor.U16();
                break;
            case kJunctionOffset:
                tCursor.Skip(4);
                break;
            case kSetDefinition8:
                tDefinition = tCursor.U8();
                break;
            case kSetDefinition16:
                tDefinition = tCursor.U16();
                break;
            case kSetDefinition32:
                tDefinition = static_cast<int>(tCursor.U32());
                break;
            case kRoadSubtype8:
                tCursor.Skip(1);
                break;
            case kObject:
                tCursor.Skip(2);
                break;
            case kObjectRange:
                tCursor.Skip(4);
                break;
            case kNetworkChain:
                tCursor.Skip(static_cast<size_t>(tCursor.U8()) * 2);
                break;
            case kNetworkChainRange:
                tCursor.Skip(4);
                break;
            case kNetworkChain32:
                tCursor.Skip(static_cast<size_t>(tCursor.U8()) * 4);
                break;
            case kPolygon:
                tCursor.Skip(2);
                tCursor.Skip(static_cast<size_t>(tCursor.U8()) * 2);
                break;
            case kPolygonRange:
                tCursor.Skip(6);
                break;
            case kNestedPolygon:
            {
                tCursor.Skip(2);
                const int tWindings = tCursor.U8();
                for (int tWinding = 0; tWinding < tWindings; ++tWinding)
                {
                    tCursor.Skip(static_cast<size_t>(tCursor.U8()) * 2);
                }
                break;
            }
            case kNestedPolygonRange:
            {
                // Belge burada belirsiz; gercek XP11 dosyasinda sayac N iken N + 1
                // indeks var (her sargi bir onceki indeksten bir sonrakine kadar)
                tCursor.Skip(2);
                const size_t tCount = tCursor.U8();
                tCursor.Skip((tCount + 1) * 2);
                break;
            }
            case kTerrainPatch:
                tPatchTerrain = tDefinition;
                tCorner = 0;
                break;
            case kTerrainPatchFlags:
                tPatchFlags = tCursor.U8();
                tPatchTerrain = tDefinition;
                tCorner = 0;
                break;
            case kTerrainPatchFlagsLod:
                tPatchFlags = tCursor.U8();
                tCursor.Skip(8);  // yakin / uzak LOD (float)
                tPatchTerrain = tDefinition;
                tCorner = 0;
                break;
            case kTriangle:
            {
                const int tCount = tCursor.U8();
                for (int tIndex = 0; tIndex < tCount; ++tIndex)
                {
                    tEmit(tPool, tCursor.U16());
                }
                break;
            }
            case kTriangleCrossPool:
            {
                const int tCount = tCursor.U8();
                for (int tIndex = 0; tIndex < tCount; ++tIndex)
                {
                    const int tPoolIndex = tCursor.U16();
                    tEmit(tPoolIndex, tCursor.U16());
                }
                break;
            }
            case kTriangleRange:
            {
                const int tFirst = tCursor.U16();
                const int tLast = tCursor.U16();
                for (int tIndex = tFirst; tIndex < tLast; ++tIndex)
                {
                    tEmit(tPool, tIndex);
                }
                break;
            }
            case kTriangleStrip:
            case kTriangleFan:
            {
                const int tCount = tCursor.U8();
                tVertices.clear();
                for (int tIndex = 0; tIndex < tCount; ++tIndex)
                {
                    tVertices.emplace_back(tPool, tCursor.U16());
                }
                if (tCommand == kTriangleStrip)
                {
                    tEmitStrip();
                }
                else
                {
                    tEmitFan();
                }
                break;
            }
            case kTriangleStripCrossPool:
            case kTriangleFanCrossPool:
            {
                const int tCount = tCursor.U8();
                tVertices.clear();
                for (int tIndex = 0; tIndex < tCount; ++tIndex)
                {
                    const int tPoolIndex = tCursor.U16();
                    tVertices.emplace_back(tPoolIndex, tCursor.U16());
                }
                if (tCommand == kTriangleStripCrossPool)
                {
                    tEmitStrip();
                }
                else
                {
                    tEmitFan();
                }
                break;
            }
            case kTriangleStripRange:
            case kTriangleFanRange:
            {
                const int tFirst = tCursor.U16();
                const int tLast = tCursor.U16();
                tVertices.clear();
                for (int tIndex = tFirst; tIndex < tLast; ++tIndex)
                {
                    tVertices.emplace_back(tPool, tIndex);
                }
                if (tCommand == kTriangleStripRange)
                {
                    tEmitStrip();
                }
                else
                {
                    tEmitFan();
                }
                break;
            }
            case kComment8:
                tCursor.Skip(tCursor.U8());
                break;
            case kComment16:
                tCursor.Skip(tCursor.U16());
                break;
            case kComment32:
                tCursor.Skip(tCursor.U32());
                break;
            default:
                // Bicim: bilinmeyen komutun uzunlugu bilinemez, okumaya devam edilemez
                *pError = "unknown DSF command " + std::to_string(tCommand);
                return false;
        }
    }
    if (tCursor.mFailed || tBadIndex)
    {
        *pError = tBadIndex ? "DSF command refers to a missing point" : "DSF command stream is truncated";
        return false;
    }
    return true;
}

bool DsfReader::CountCommands(DsfCommandStatistics* pOut, std::string* pError) const
{
    *pOut = DsfCommandStatistics();
    pOut->mObjects.assign(mInfo.mObjectNames.size(), 0);
    pOut->mPolygons.assign(mInfo.mPolygonNames.size(), {});
    if (!mData || mCommandsSize == 0)
    {
        return true;
    }
    Cursor tCursor{mData->data() + mCommandsOffset, mCommandsSize};
    int tDefinition = 0;
    int tRoadSubtype = 0;

    // Tanim indeksi tablonun disindaysa sayilmaz (bozuk dosyada cokme olmasin)
    auto tCountObjects = [&](long long pCount)
    {
        if (tDefinition >= 0 && tDefinition < static_cast<int>(pOut->mObjects.size()))
        {
            pOut->mObjects[static_cast<size_t>(tDefinition)] += pCount;
        }
    };
    auto tCountPolygon = [&](long long pWindings, long long pPoints)
    {
        if (tDefinition >= 0 && tDefinition < static_cast<int>(pOut->mPolygons.size()))
        {
            DsfCommandStatistics::PolygonCount& tCount = pOut->mPolygons[static_cast<size_t>(tDefinition)];
            tCount.mPolygons += 1;
            tCount.mWindings += pWindings;
            tCount.mPoints += pPoints;
        }
    };
    auto tCountChain = [&](long long pPoints)
    {
        const std::pair<int, int> tKey(tDefinition, tRoadSubtype);
        for (auto& tEntry : pOut->mChains)
        {
            if (tEntry.first == tKey)
            {
                tEntry.second.mChains += 1;
                tEntry.second.mPoints += pPoints;
                return;
            }
        }
        DsfCommandStatistics::ChainCount tCount;
        tCount.mChains = 1;
        tCount.mPoints = pPoints;
        pOut->mChains.emplace_back(tKey, tCount);
    };

    while (tCursor.mPosition < tCursor.mSize && !tCursor.mFailed)
    {
        const uint8_t tCommand = tCursor.U8();
        switch (tCommand)
        {
            case kPoolSelect:
                tCursor.Skip(2);
                break;
            case kJunctionOffset:
                tCursor.Skip(4);
                break;
            case kSetDefinition8:
                tDefinition = tCursor.U8();
                break;
            case kSetDefinition16:
                tDefinition = tCursor.U16();
                break;
            case kSetDefinition32:
                tDefinition = static_cast<int>(tCursor.U32());
                break;
            case kRoadSubtype8:
                tRoadSubtype = tCursor.U8();
                break;
            case kObject:
                tCursor.Skip(2);
                tCountObjects(1);
                break;
            case kObjectRange:
            {
                const int tFirst = tCursor.U16();
                const int tLast = tCursor.U16();
                tCountObjects(tLast - tFirst);
                break;
            }
            case kNetworkChain:
            {
                const int tCount = tCursor.U8();
                tCursor.Skip(static_cast<size_t>(tCount) * 2);
                tCountChain(tCount);
                break;
            }
            case kNetworkChainRange:
            {
                const int tFirst = tCursor.U16();
                const int tLast = tCursor.U16();
                tCountChain(tLast - tFirst);
                break;
            }
            case kNetworkChain32:
            {
                const int tCount = tCursor.U8();
                tCursor.Skip(static_cast<size_t>(tCount) * 4);
                tCountChain(tCount);
                break;
            }
            case kPolygon:
            {
                tCursor.Skip(2);  // parametre (orman yogunlugu, cephe yuksekligi ...)
                const int tCount = tCursor.U8();
                tCursor.Skip(static_cast<size_t>(tCount) * 2);
                tCountPolygon(1, tCount);
                break;
            }
            case kPolygonRange:
            {
                tCursor.Skip(2);
                const int tFirst = tCursor.U16();
                const int tLast = tCursor.U16();
                tCountPolygon(1, tLast - tFirst);
                break;
            }
            case kNestedPolygon:
            {
                tCursor.Skip(2);
                const int tWindings = tCursor.U8();
                long long tPoints = 0;
                for (int tWinding = 0; tWinding < tWindings; ++tWinding)
                {
                    const int tCount = tCursor.U8();
                    tCursor.Skip(static_cast<size_t>(tCount) * 2);
                    tPoints += tCount;
                }
                tCountPolygon(tWindings, tPoints);
                break;
            }
            case kNestedPolygonRange:
            {
                // GERCEK VERI: sayac N iken N + 1 indeks (bkz. ForEachTriangle)
                tCursor.Skip(2);
                const size_t tCount = tCursor.U8();
                int tFirst = 0;
                int tLast = 0;
                for (size_t tIndex = 0; tIndex <= tCount; ++tIndex)
                {
                    const int tValue = tCursor.U16();
                    tFirst = (tIndex == 0) ? tValue : tFirst;
                    tLast = tValue;
                }
                tCountPolygon(static_cast<long long>(tCount), tLast - tFirst);
                break;
            }
            case kTerrainPatch:
                pOut->mTerrainPatches += 1;
                break;
            case kTerrainPatchFlags:
                tCursor.Skip(1);
                pOut->mTerrainPatches += 1;
                break;
            case kTerrainPatchFlagsLod:
                tCursor.Skip(9);
                pOut->mTerrainPatches += 1;
                break;
            case kTriangle:
            case kTriangleStrip:
            case kTriangleFan:
                tCursor.Skip(static_cast<size_t>(tCursor.U8()) * 2);
                break;
            case kTriangleCrossPool:
            case kTriangleStripCrossPool:
            case kTriangleFanCrossPool:
                tCursor.Skip(static_cast<size_t>(tCursor.U8()) * 4);
                break;
            case kTriangleRange:
            case kTriangleStripRange:
            case kTriangleFanRange:
                tCursor.Skip(4);
                break;
            case kComment8:
                tCursor.Skip(tCursor.U8());
                break;
            case kComment16:
                tCursor.Skip(tCursor.U16());
                break;
            case kComment32:
                tCursor.Skip(tCursor.U32());
                break;
            default:
                *pError = "unknown DSF command " + std::to_string(tCommand);
                return false;
        }
    }
    if (tCursor.mFailed)
    {
        *pError = "DSF command stream is truncated";
        return false;
    }
    return true;
}

bool DsfReader::ForEachVector(const std::function<void(const DsfPolygon&)>& pPolygonVisitor,
                              const std::function<void(const DsfChain&)>& pChainVisitor, std::string* pError) const
{
    if (!mData || mCommandsSize == 0)
    {
        return true;
    }
    Cursor tCursor{mData->data() + mCommandsOffset, mCommandsSize};
    int tPool = 0;
    uint32_t tJunctionOffset = 0;
    int tDefinition = 0;
    int tRoadSubtype = 0;
    bool tBadIndex = false;

    // 16 bitlik havuzdan (poligonlar) bir nokta
    auto tPoint16 = [&](int pIndex, double* pLon, double* pLat)
    {
        if (tPool < 0 || tPool >= static_cast<int>(mPools.size()) || pIndex < 0 || pIndex >= mPools[tPool].mCount ||
            mPools[tPool].mPlaneCount < 2)
        {
            tBadIndex = true;
            return;
        }
        *pLon = PlaneValue(mPools[tPool], 0, pIndex);
        *pLat = PlaneValue(mPools[tPool], 1, pIndex);
    };
    // 32 bitlik havuzdan (zincirler) bir nokta; olcek 16 bitlikle ayni kural: ham / (2^32 - 1) * olcek + kaydirma
    auto tPoint32 = [&](uint32_t pIndex, double* pLon, double* pLat)
    {
        if (tPool < 0 || tPool >= static_cast<int>(mPools32.size()) ||
            pIndex >= static_cast<uint32_t>(mPools32[tPool].mCount) || mPools32[tPool].mPlaneCount < 2)
        {
            tBadIndex = true;
            return;
        }
        const Pool32& tSource = mPools32[tPool];
        double tValue[2];
        for (int tPlane = 0; tPlane < 2; ++tPlane)
        {
            const double tRaw = tSource.mPlanes[tPlane][pIndex];
            tValue[tPlane] = tSource.mScale[tPlane] == 0.0
                                 ? tRaw
                                 : tRaw / 4294967295.0 * tSource.mScale[tPlane] + tSource.mOffset[tPlane];
        }
        *pLon = tValue[0];
        *pLat = tValue[1];
    };

    DsfPolygon tPolygon;
    auto tBeginPolygon = [&](int pParameter)
    {
        tPolygon.mDefinition = tDefinition;
        tPolygon.mParameter = pParameter;
        tPolygon.mRingLon.clear();
        tPolygon.mRingLat.clear();
    };
    auto tAddRing = [&]()
    {
        tPolygon.mRingLon.emplace_back();
        tPolygon.mRingLat.emplace_back();
    };
    auto tAddRingPoint = [&](int pIndex)
    {
        double tLon = 0.0;
        double tLat = 0.0;
        tPoint16(pIndex, &tLon, &tLat);
        tPolygon.mRingLon.back().push_back(tLon);
        tPolygon.mRingLat.back().push_back(tLat);
    };
    auto tEmitPolygon = [&]()
    {
        if (pPolygonVisitor && !tBadIndex)
        {
            pPolygonVisitor(tPolygon);
        }
    };

    DsfChain tChain;
    auto tBeginChain = [&]()
    {
        tChain.mDefinition = tDefinition;
        tChain.mSubtype = tRoadSubtype;
        tChain.mLon.clear();
        tChain.mLat.clear();
    };
    auto tAddChainPoint = [&](uint32_t pIndex)
    {
        double tLon = 0.0;
        double tLat = 0.0;
        tPoint32(pIndex, &tLon, &tLat);
        tChain.mLon.push_back(tLon);
        tChain.mLat.push_back(tLat);
    };
    auto tEmitChain = [&]()
    {
        if (pChainVisitor && !tBadIndex && tChain.mLon.size() >= 2)
        {
            pChainVisitor(tChain);
        }
    };

    while (tCursor.mPosition < tCursor.mSize && !tCursor.mFailed && !tBadIndex)
    {
        const uint8_t tCommand = tCursor.U8();
        switch (tCommand)
        {
            case kPoolSelect:
                tPool = tCursor.U16();
                break;
            case kJunctionOffset:
                tJunctionOffset = tCursor.U32();
                break;
            case kSetDefinition8:
                tDefinition = tCursor.U8();
                break;
            case kSetDefinition16:
                tDefinition = tCursor.U16();
                break;
            case kSetDefinition32:
                tDefinition = static_cast<int>(tCursor.U32());
                break;
            case kRoadSubtype8:
                tRoadSubtype = tCursor.U8();
                break;
            case kObject:
                tCursor.Skip(2);
                break;
            case kObjectRange:
                tCursor.Skip(4);
                break;
            case kNetworkChain:
            {
                // Belge: 9 ve 10 indekslere junction offset'i ekler, 11 eklemez
                const int tCount = tCursor.U8();
                tBeginChain();
                for (int tIndex = 0; tIndex < tCount; ++tIndex)
                {
                    tAddChainPoint(tJunctionOffset + tCursor.U16());
                }
                tEmitChain();
                break;
            }
            case kNetworkChainRange:
            {
                const uint32_t tFirst = tCursor.U16();
                const uint32_t tLast = tCursor.U16();
                tBeginChain();
                for (uint32_t tIndex = tFirst; tIndex < tLast; ++tIndex)
                {
                    tAddChainPoint(tJunctionOffset + tIndex);
                }
                tEmitChain();
                break;
            }
            case kNetworkChain32:
            {
                const int tCount = tCursor.U8();
                tBeginChain();
                for (int tIndex = 0; tIndex < tCount; ++tIndex)
                {
                    tAddChainPoint(tCursor.U32());
                }
                tEmitChain();
                break;
            }
            case kPolygon:
            {
                tBeginPolygon(tCursor.U16());
                const int tCount = tCursor.U8();
                tAddRing();
                for (int tIndex = 0; tIndex < tCount; ++tIndex)
                {
                    tAddRingPoint(tCursor.U16());
                }
                tEmitPolygon();
                break;
            }
            case kPolygonRange:
            {
                tBeginPolygon(tCursor.U16());
                const int tFirst = tCursor.U16();
                const int tLast = tCursor.U16();
                tAddRing();
                for (int tIndex = tFirst; tIndex < tLast; ++tIndex)
                {
                    tAddRingPoint(tIndex);
                }
                tEmitPolygon();
                break;
            }
            case kNestedPolygon:
            {
                tBeginPolygon(tCursor.U16());
                const int tWindings = tCursor.U8();
                for (int tWinding = 0; tWinding < tWindings; ++tWinding)
                {
                    const int tCount = tCursor.U8();
                    tAddRing();
                    for (int tIndex = 0; tIndex < tCount; ++tIndex)
                    {
                        tAddRingPoint(tCursor.U16());
                    }
                }
                tEmitPolygon();
                break;
            }
            case kNestedPolygonRange:
            {
                // GERCEK VERI: sayac N iken N + 1 indeks; halka k = [indeks k, indeks k + 1)
                tBeginPolygon(tCursor.U16());
                const int tCount = tCursor.U8();
                std::vector<int> tBounds;
                for (int tIndex = 0; tIndex <= tCount; ++tIndex)
                {
                    tBounds.push_back(tCursor.U16());
                }
                for (int tWinding = 0; tWinding < tCount; ++tWinding)
                {
                    tAddRing();
                    for (int tIndex = tBounds[tWinding]; tIndex < tBounds[tWinding + 1]; ++tIndex)
                    {
                        tAddRingPoint(tIndex);
                    }
                }
                tEmitPolygon();
                break;
            }
            case kTerrainPatch:
                break;
            case kTerrainPatchFlags:
                tCursor.Skip(1);
                break;
            case kTerrainPatchFlagsLod:
                tCursor.Skip(9);
                break;
            case kTriangle:
            case kTriangleStrip:
            case kTriangleFan:
                tCursor.Skip(static_cast<size_t>(tCursor.U8()) * 2);
                break;
            case kTriangleCrossPool:
            case kTriangleStripCrossPool:
            case kTriangleFanCrossPool:
                tCursor.Skip(static_cast<size_t>(tCursor.U8()) * 4);
                break;
            case kTriangleRange:
            case kTriangleStripRange:
            case kTriangleFanRange:
                tCursor.Skip(4);
                break;
            case kComment8:
                tCursor.Skip(tCursor.U8());
                break;
            case kComment16:
                tCursor.Skip(tCursor.U16());
                break;
            case kComment32:
                tCursor.Skip(tCursor.U32());
                break;
            default:
                *pError = "unknown DSF command " + std::to_string(tCommand);
                return false;
        }
    }
    if (tCursor.mFailed || tBadIndex)
    {
        *pError = tBadIndex ? "DSF vector command refers to a missing point" : "DSF command stream is truncated";
        return false;
    }
    return true;
}
