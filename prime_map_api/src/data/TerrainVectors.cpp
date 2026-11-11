// TerrainVectors.cpp
#include "data/TerrainVectors.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>

#include "data/DsfReader.h"
#include "data/ElevationCodec.h"

namespace
{

const char kMagic[4] = {'M', 'P', 'V', 'C'};
const uint32_t kFileVersion = 1;
const double kMetersPerDegree = 111320.0;
const double kPi = 3.14159265358979323846;
const double kLineToleranceMeters = 2.5;  // yol / demiryolu sadelestirme (z16.5'te ~1 piksel)
const double kAreaToleranceMeters = 4.0;  // orman / yerlesim halkalari
const float kQuantum = 65535.0f;          // noktalar karo icinde 16 bit (~1,7 m)

// Dosya adinin kucuk harf hali (son '/' sonrasi)
std::string FileNameLower(const std::string& pPath)
{
    const size_t tSlash = pPath.find_last_of("/\\");
    std::string tName = (tSlash == std::string::npos) ? pPath : pPath.substr(tSlash + 1);
    for (char& tChar : tName)
    {
        tChar = static_cast<char>(std::tolower(static_cast<unsigned char>(tChar)));
    }
    return tName;
}

bool EndsWith(const std::string& pText, const char* pSuffix)
{
    const size_t tLength = std::strlen(pSuffix);
    return pText.size() >= tLength && pText.compare(pText.size() - tLength, tLength, pSuffix) == 0;
}

bool StartsWith(const std::string& pText, const char* pPrefix)
{
    return pText.rfind(pPrefix, 0) == 0;
}

// Douglas-Peucker: pKeep[i] = noktayi tut. x olcegi pScaleX (boylam -> enlemle ayni metre), tolerans karo birimi.
void Simplify(const std::vector<double>& pX, const std::vector<double>& pY, double pScaleX, double pTolerance,
              std::vector<uint8_t>* pKeep)
{
    const size_t tCount = pX.size();
    pKeep->assign(tCount, 0);
    if (tCount <= 2)
    {
        pKeep->assign(tCount, 1);
        return;
    }
    (*pKeep)[0] = 1;
    (*pKeep)[tCount - 1] = 1;
    std::vector<std::pair<size_t, size_t>> tStack = {{0, tCount - 1}};
    const double tToleranceSquared = pTolerance * pTolerance;
    while (!tStack.empty())
    {
        const auto [tFirst, tLast] = tStack.back();
        tStack.pop_back();
        const double tAx = pX[tFirst] * pScaleX;
        const double tAy = pY[tFirst];
        const double tDx = pX[tLast] * pScaleX - tAx;
        const double tDy = pY[tLast] - tAy;
        const double tLengthSquared = tDx * tDx + tDy * tDy;
        double tWorst = -1.0;
        size_t tWorstIndex = tFirst;
        for (size_t tIndex = tFirst + 1; tIndex < tLast; ++tIndex)
        {
            const double tPx = pX[tIndex] * pScaleX - tAx;
            const double tPy = pY[tIndex] - tAy;
            double tDistanceSquared = 0.0;
            if (tLengthSquared <= 0.0)
            {
                tDistanceSquared = tPx * tPx + tPy * tPy;
            }
            else
            {
                const double tCross = tPx * tDy - tPy * tDx;
                tDistanceSquared = tCross * tCross / tLengthSquared;
            }
            if (tDistanceSquared > tWorst)
            {
                tWorst = tDistanceSquared;
                tWorstIndex = tIndex;
            }
        }
        if (tWorst > tToleranceSquared)
        {
            (*pKeep)[tWorstIndex] = 1;
            tStack.push_back({tFirst, tWorstIndex});
            tStack.push_back({tWorstIndex, tLast});
        }
    }
}

uint16_t Quantize(float pValue)
{
    return static_cast<uint16_t>(std::lround(std::clamp(pValue, 0.0f, 1.0f) * kQuantum));
}

// Bayt yazici / okuyucu (kucuk-endian; sadece bu dosya bicimi icin)
struct Writer
{
    std::vector<uint8_t> mBytes;

    void U8(uint8_t pValue)
    {
        mBytes.push_back(pValue);
    }
    void U16(uint16_t pValue)
    {
        mBytes.push_back(static_cast<uint8_t>(pValue & 0xFF));
        mBytes.push_back(static_cast<uint8_t>(pValue >> 8));
    }
    void U32(uint32_t pValue)
    {
        for (int tShift = 0; tShift < 32; tShift += 8)
        {
            mBytes.push_back(static_cast<uint8_t>((pValue >> tShift) & 0xFF));
        }
    }
    void Points(const std::vector<float>& pX, const std::vector<float>& pY)
    {
        for (size_t tIndex = 0; tIndex < pX.size(); ++tIndex)
        {
            U16(Quantize(pX[tIndex]));
            U16(Quantize(pY[tIndex]));
        }
    }
};

struct Reader
{
    const std::vector<uint8_t>& mBytes;
    size_t mPosition = 0;
    bool mFailed = false;

    bool Has(size_t pCount)
    {
        mFailed = mFailed || mPosition + pCount > mBytes.size();
        return !mFailed;
    }
    uint8_t U8()
    {
        return Has(1) ? mBytes[mPosition++] : 0;
    }
    uint16_t U16()
    {
        if (!Has(2))
        {
            return 0;
        }
        const uint16_t tValue = static_cast<uint16_t>(mBytes[mPosition] | (mBytes[mPosition + 1] << 8));
        mPosition += 2;
        return tValue;
    }
    uint32_t U32()
    {
        uint32_t tValue = 0;
        for (int tShift = 0; tShift < 32 && Has(1); tShift += 8)
        {
            tValue |= static_cast<uint32_t>(mBytes[mPosition++]) << tShift;
        }
        return tValue;
    }
    void Points(uint32_t pCount, std::vector<float>* pX, std::vector<float>* pY)
    {
        if (!Has(static_cast<size_t>(pCount) * 4))
        {
            return;
        }
        for (uint32_t tIndex = 0; tIndex < pCount; ++tIndex)
        {
            pX->push_back(U16() / kQuantum);
            pY->push_back(U16() / kQuantum);
        }
    }
};

}  // namespace

void TerrainVectors::Reset(int pWest, int pSouth)
{
    mWest = pWest;
    mSouth = pSouth;
    mAreas.clear();
    mLines.clear();
    mExclusions.clear();
}

int TerrainVectors::West() const
{
    return mWest;
}

int TerrainVectors::South() const
{
    return mSouth;
}

TerrainClass TerrainVectors::AreaClassFor(const std::string& pDefinitionName)
{
    const std::string tName = FileNameLower(pDefinitionName);
    if (EndsWith(tName, ".for"))
    {
        // Orman ailesi: arazi turu on ek kurallari (coni, broad, shrb, wetl, orch ...); digerleri genis yaprakli
        const TerrainClass tClass = ClassifyTerrain(tName);
        const bool tVegetation = tClass == TerrainClass::ForestConifer || tClass == TerrainClass::ForestBroadleaf ||
                                 tClass == TerrainClass::Shrub || tClass == TerrainClass::Wetland ||
                                 tClass == TerrainClass::Crop;
        return tVegetation ? tClass : TerrainClass::ForestBroadleaf;
    }
    if (EndsWith(tName, ".ags"))
    {
        if (StartsWith(tName, "urban_") || StartsWith(tName, "ind_") || StartsWith(tName, "com_"))
        {
            return TerrainClass::Urban;
        }
        if (StartsWith(tName, "park_"))
        {
            return TerrainClass::Grass;
        }
    }
    return TerrainClass::None;  // natural.ags, .fac, .bch, .pol, .lin ...
}

int TerrainVectors::LineKindFor(int pSubtype)
{
    if (pSubtype >= 100 && pSubtype <= 131)
    {
        return static_cast<int>(VectorLineKind::Motorway);
    }
    if (pSubtype >= 10 && pSubtype <= 28)
    {
        return static_cast<int>(VectorLineKind::Primary);
    }
    if (pSubtype >= 30 && pSubtype <= 48)
    {
        return static_cast<int>(VectorLineKind::Secondary);
    }
    if (pSubtype >= 50 && pSubtype <= 71)
    {
        return static_cast<int>(VectorLineKind::Local);
    }
    if (pSubtype >= 151 && pSubtype <= 154)
    {
        return static_cast<int>(VectorLineKind::Railway);
    }
    if (pSubtype == 220)
    {
        return static_cast<int>(VectorLineKind::PowerLine);
    }
    return -1;
}

bool TerrainVectors::IsForestClass(TerrainClass pClass)
{
    return pClass == TerrainClass::ForestConifer || pClass == TerrainClass::ForestBroadleaf ||
           pClass == TerrainClass::Shrub || pClass == TerrainClass::Wetland || pClass == TerrainClass::Crop;
}

bool TerrainVectors::AddFromDsf(const DsfReader& pReader, std::string* pError)
{
    const DsfInfo& tInfo = pReader.Info();
    std::vector<TerrainClass> tAreaClasses;
    for (const std::string& tName : tInfo.mPolygonNames)
    {
        tAreaClasses.push_back(AreaClassFor(tName));
    }
    std::vector<bool> tIsRoadNetwork;
    for (const std::string& tName : tInfo.mNetworkNames)
    {
        tIsRoadNetwork.push_back(EndsWith(FileNameLower(tName), ".net"));
    }
    // Sadelestirme toleransi karo biriminde; x, enlem kosinusuyla olceklenir (metre esit)
    const double tScaleX = std::max(0.05, std::cos((mSouth + 0.5) * kPi / 180.0));
    const double tLineTolerance = kLineToleranceMeters / kMetersPerDegree;
    const double tAreaTolerance = kAreaToleranceMeters / kMetersPerDegree;
    std::vector<uint8_t> tKeep;
    std::vector<double> tX;
    std::vector<double> tY;

    auto tOnPolygon = [&](const DsfPolygon& pPolygon)
    {
        if (pPolygon.mDefinition < 0 || pPolygon.mDefinition >= static_cast<int>(tAreaClasses.size()) ||
            tAreaClasses[pPolygon.mDefinition] == TerrainClass::None)
        {
            return;
        }
        VectorArea tArea;
        tArea.mClass = tAreaClasses[pPolygon.mDefinition];
        for (size_t tRing = 0; tRing < pPolygon.mRingLon.size(); ++tRing)
        {
            tX.clear();
            tY.clear();
            for (size_t tIndex = 0; tIndex < pPolygon.mRingLon[tRing].size(); ++tIndex)
            {
                tX.push_back(pPolygon.mRingLon[tRing][tIndex] - mWest);
                tY.push_back(pPolygon.mRingLat[tRing][tIndex] - mSouth);
            }
            Simplify(tX, tY, tScaleX, tAreaTolerance, &tKeep);
            tArea.mRingStarts.push_back(static_cast<uint32_t>(tArea.mX.size()));
            for (size_t tIndex = 0; tIndex < tX.size(); ++tIndex)
            {
                if (!tKeep[tIndex])
                {
                    continue;
                }
                tArea.mX.push_back(static_cast<float>(tX[tIndex]));
                tArea.mY.push_back(static_cast<float>(tY[tIndex]));
                tArea.mBounds[0] = std::min(tArea.mBounds[0], static_cast<float>(tX[tIndex]));
                tArea.mBounds[1] = std::min(tArea.mBounds[1], static_cast<float>(tY[tIndex]));
                tArea.mBounds[2] = std::max(tArea.mBounds[2], static_cast<float>(tX[tIndex]));
                tArea.mBounds[3] = std::max(tArea.mBounds[3], static_cast<float>(tY[tIndex]));
            }
        }
        if (tArea.mX.size() >= 3)
        {
            mAreas.push_back(std::move(tArea));
        }
    };
    auto tOnChain = [&](const DsfChain& pChain)
    {
        const int tKind = LineKindFor(pChain.mSubtype);
        if (tKind < 0 || pChain.mDefinition < 0 || pChain.mDefinition >= static_cast<int>(tIsRoadNetwork.size()) ||
            !tIsRoadNetwork[pChain.mDefinition])
        {
            return;
        }
        tX.clear();
        tY.clear();
        for (size_t tIndex = 0; tIndex < pChain.mLon.size(); ++tIndex)
        {
            tX.push_back(pChain.mLon[tIndex] - mWest);
            tY.push_back(pChain.mLat[tIndex] - mSouth);
        }
        Simplify(tX, tY, tScaleX, tLineTolerance, &tKeep);
        VectorLine tLine;
        tLine.mKind = static_cast<VectorLineKind>(tKind);
        for (size_t tIndex = 0; tIndex < tX.size(); ++tIndex)
        {
            if (tKeep[tIndex])
            {
                tLine.mX.push_back(static_cast<float>(tX[tIndex]));
                tLine.mY.push_back(static_cast<float>(tY[tIndex]));
            }
        }
        mLines.push_back(std::move(tLine));
    };
    return pReader.ForEachVector(tOnPolygon, tOnChain, pError);
}

void TerrainVectors::AddExclusions(const DsfInfo& pOverlayInfo)
{
    for (const auto& [tName, tValue] : pOverlayInfo.mProperties)
    {
        uint8_t tKinds = 0;
        if (tName == "sim/exclude_for")
        {
            tKinds = kExcludeForest;
        }
        else if (tName == "sim/exclude_obj" || tName == "sim/exclude_fac")
        {
            tKinds = kExcludeAutogen;
        }
        else if (tName == "sim/exclude_net")
        {
            tKinds = kExcludeNetwork;
        }
        else
        {
            continue;
        }
        // GERCEK VERI: "bati/guney/dogu/kuzey" (derece; nokta ondalik, yerel ayardan bagimsiz okunur)
        double tBounds[4] = {0.0, 0.0, 0.0, 0.0};
        const char* tCursor = tValue.c_str();
        bool tOk = true;
        for (int tPart = 0; tPart < 4 && tOk; ++tPart)
        {
            char* tEnd = nullptr;
            tBounds[tPart] = std::strtod(tCursor, &tEnd);  // "C" yerel ayari: eklenti ayari degistirmez
            tOk = tEnd != tCursor;
            tCursor = (*tEnd == '/') ? tEnd + 1 : tEnd;
        }
        if (!tOk)
        {
            continue;
        }
        VectorExclusion tExclusion;
        tExclusion.mBounds = {static_cast<float>(tBounds[0] - mWest), static_cast<float>(tBounds[1] - mSouth),
                              static_cast<float>(tBounds[2] - mWest), static_cast<float>(tBounds[3] - mSouth)};
        tExclusion.mKinds = tKinds;
        mExclusions.push_back(tExclusion);
    }
}

void TerrainVectors::ApplyNetworkExclusions()
{
    auto tExcluded = [&](float pX, float pY)
    {
        for (const VectorExclusion& tExclusion : mExclusions)
        {
            if ((tExclusion.mKinds & kExcludeNetwork) && pX >= tExclusion.mBounds[0] && pX <= tExclusion.mBounds[2] &&
                pY >= tExclusion.mBounds[1] && pY <= tExclusion.mBounds[3])
            {
                return true;
            }
        }
        return false;
    };
    bool tAny = false;
    for (const VectorExclusion& tExclusion : mExclusions)
    {
        tAny = tAny || (tExclusion.mKinds & kExcludeNetwork) != 0;
    }
    if (!tAny)
    {
        return;
    }
    // Ortasi dislama alaninda kalan parcalar atilir; cizgi orada bolunur
    std::vector<VectorLine> tResult;
    for (const VectorLine& tLine : mLines)
    {
        VectorLine tPart;
        tPart.mKind = tLine.mKind;
        for (size_t tIndex = 0; tIndex + 1 < tLine.mX.size(); ++tIndex)
        {
            const float tMidX = (tLine.mX[tIndex] + tLine.mX[tIndex + 1]) * 0.5f;
            const float tMidY = (tLine.mY[tIndex] + tLine.mY[tIndex + 1]) * 0.5f;
            if (tExcluded(tMidX, tMidY))
            {
                if (tPart.mX.size() >= 2)
                {
                    tResult.push_back(tPart);
                }
                tPart.mX.clear();
                tPart.mY.clear();
                continue;
            }
            if (tPart.mX.empty())
            {
                tPart.mX.push_back(tLine.mX[tIndex]);
                tPart.mY.push_back(tLine.mY[tIndex]);
            }
            tPart.mX.push_back(tLine.mX[tIndex + 1]);
            tPart.mY.push_back(tLine.mY[tIndex + 1]);
        }
        if (tPart.mX.size() >= 2)
        {
            tResult.push_back(std::move(tPart));
        }
    }
    mLines.swap(tResult);
}

const std::vector<VectorArea>& TerrainVectors::Areas() const
{
    return mAreas;
}

const std::vector<VectorLine>& TerrainVectors::Lines() const
{
    return mLines;
}

bool TerrainVectors::IsAreaExcluded(TerrainClass pClass, double pX, double pY) const
{
    const uint8_t tKind = IsForestClass(pClass) ? kExcludeForest : kExcludeAutogen;
    for (const VectorExclusion& tExclusion : mExclusions)
    {
        if ((tExclusion.mKinds & tKind) && pX >= tExclusion.mBounds[0] && pX <= tExclusion.mBounds[2] &&
            pY >= tExclusion.mBounds[1] && pY <= tExclusion.mBounds[3])
        {
            return true;
        }
    }
    return false;
}

size_t TerrainVectors::PointCount() const
{
    size_t tCount = 0;
    for (const VectorArea& tArea : mAreas)
    {
        tCount += tArea.mX.size();
    }
    for (const VectorLine& tLine : mLines)
    {
        tCount += tLine.mX.size();
    }
    return tCount;
}

bool TerrainVectors::Save(const std::string& pPath, const std::string& pKey) const
{
    Writer tWriter;
    tWriter.U32(static_cast<uint32_t>(mAreas.size()));
    for (const VectorArea& tArea : mAreas)
    {
        tWriter.U8(static_cast<uint8_t>(tArea.mClass));
        tWriter.U32(static_cast<uint32_t>(tArea.mRingStarts.size()));
        for (const uint32_t tStart : tArea.mRingStarts)
        {
            tWriter.U32(tStart);
        }
        tWriter.U32(static_cast<uint32_t>(tArea.mX.size()));
        tWriter.Points(tArea.mX, tArea.mY);
    }
    tWriter.U32(static_cast<uint32_t>(mLines.size()));
    for (const VectorLine& tLine : mLines)
    {
        tWriter.U8(static_cast<uint8_t>(tLine.mKind));
        tWriter.U32(static_cast<uint32_t>(tLine.mX.size()));
        tWriter.Points(tLine.mX, tLine.mY);
    }
    tWriter.U32(static_cast<uint32_t>(mExclusions.size()));
    for (const VectorExclusion& tExclusion : mExclusions)
    {
        tWriter.U8(tExclusion.mKinds);
        for (const float tValue : tExclusion.mBounds)
        {
            uint32_t tBits = 0;
            std::memcpy(&tBits, &tValue, sizeof(tBits));
            tWriter.U32(tBits);
        }
    }
    std::vector<uint8_t> tPacked;
    if (!ElevationCodec::PackBytes(tWriter.mBytes, &tPacked))
    {
        return false;
    }
    std::ofstream tFile(pPath, std::ios::binary | std::ios::trunc);
    if (!tFile)
    {
        return false;
    }
    const uint32_t tHeader[3] = {kFileVersion, static_cast<uint32_t>(pKey.size()),
                                 static_cast<uint32_t>(mWest + 1000) | (static_cast<uint32_t>(mSouth + 1000) << 16)};
    tFile.write(kMagic, 4);
    tFile.write(reinterpret_cast<const char*>(tHeader), sizeof(tHeader));
    tFile.write(pKey.data(), static_cast<std::streamsize>(pKey.size()));
    tFile.write(reinterpret_cast<const char*>(tPacked.data()), static_cast<std::streamsize>(tPacked.size()));
    return static_cast<bool>(tFile);
}

bool TerrainVectors::Load(const std::string& pPath, const std::string& pKey)
{
    std::ifstream tFile(pPath, std::ios::binary);
    char tMagic[4] = {0, 0, 0, 0};
    uint32_t tHeader[3] = {0, 0, 0};
    if (!tFile.read(tMagic, 4) || std::memcmp(tMagic, kMagic, 4) != 0 ||
        !tFile.read(reinterpret_cast<char*>(tHeader), sizeof(tHeader)) || tHeader[0] != kFileVersion ||
        tHeader[1] > (1u << 20))
    {
        return false;
    }
    std::string tKey(tHeader[1], '\0');
    if (!tFile.read(&tKey[0], tHeader[1]) || tKey != pKey)
    {
        return false;
    }
    const std::vector<uint8_t> tPacked((std::istreambuf_iterator<char>(tFile)), std::istreambuf_iterator<char>());
    std::vector<uint8_t> tBytes;
    if (!ElevationCodec::UnpackBytes(tPacked.data(), tPacked.size(), &tBytes))
    {
        return false;
    }
    Reset(static_cast<int>(tHeader[2] & 0xFFFF) - 1000, static_cast<int>(tHeader[2] >> 16) - 1000);
    Reader tReader{tBytes};
    const uint32_t tAreaCount = tReader.U32();
    for (uint32_t tIndex = 0; tIndex < tAreaCount && !tReader.mFailed; ++tIndex)
    {
        VectorArea tArea;
        tArea.mClass = static_cast<TerrainClass>(std::min<int>(tReader.U8(), kTerrainClassCount - 1));
        const uint32_t tRings = tReader.U32();
        for (uint32_t tRing = 0; tRing < tRings && tReader.Has(4); ++tRing)
        {
            tArea.mRingStarts.push_back(tReader.U32());
        }
        tReader.Points(tReader.U32(), &tArea.mX, &tArea.mY);
        for (size_t tPoint = 0; tPoint < tArea.mX.size(); ++tPoint)
        {
            tArea.mBounds[0] = std::min(tArea.mBounds[0], tArea.mX[tPoint]);
            tArea.mBounds[1] = std::min(tArea.mBounds[1], tArea.mY[tPoint]);
            tArea.mBounds[2] = std::max(tArea.mBounds[2], tArea.mX[tPoint]);
            tArea.mBounds[3] = std::max(tArea.mBounds[3], tArea.mY[tPoint]);
        }
        mAreas.push_back(std::move(tArea));
    }
    const uint32_t tLineCount = tReader.U32();
    for (uint32_t tIndex = 0; tIndex < tLineCount && !tReader.mFailed; ++tIndex)
    {
        VectorLine tLine;
        tLine.mKind = static_cast<VectorLineKind>(std::min<int>(tReader.U8(), kVectorLineKindCount - 1));
        tReader.Points(tReader.U32(), &tLine.mX, &tLine.mY);
        mLines.push_back(std::move(tLine));
    }
    const uint32_t tExclusionCount = tReader.U32();
    for (uint32_t tIndex = 0; tIndex < tExclusionCount && !tReader.mFailed; ++tIndex)
    {
        VectorExclusion tExclusion;
        tExclusion.mKinds = tReader.U8();
        for (float& tValue : tExclusion.mBounds)
        {
            const uint32_t tBits = tReader.U32();
            std::memcpy(&tValue, &tBits, sizeof(tValue));
        }
        mExclusions.push_back(tExclusion);
    }
    return !tReader.mFailed;
}
