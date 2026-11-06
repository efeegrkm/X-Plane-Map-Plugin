// TerrainPaletteSource.cpp
#include "data/TerrainPaletteSource.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <vector>

namespace
{

namespace Fs = std::filesystem;

const int kMaxMipSide = 64;  // bu kenardan kucuk ilk mip seviyesi okunur (birkac KB)
const char* kTerrainFolder = "Resources/default scenery/1000 world terrain/terrain10";

uint32_t U32At(const uint8_t* pData)
{
    return static_cast<uint32_t>(pData[0]) | (static_cast<uint32_t>(pData[1]) << 8) |
           (static_cast<uint32_t>(pData[2]) << 16) | (static_cast<uint32_t>(pData[3]) << 24);
}

// RGB565 -> 8 bit kanallar
void Expand565(uint16_t pValue, int pRgb[3])
{
    pRgb[0] = ((pValue >> 11) & 31) * 255 / 31;
    pRgb[1] = ((pValue >> 5) & 63) * 255 / 63;
    pRgb[2] = (pValue & 31) * 255 / 31;
}

// DXT renk blogu (8 bayt): 4x4 pikselin renklerini toplar. pFourColor: DXT3/5 her zaman 4 renk
void AddColorBlock(const uint8_t* pBlock, bool pFourColor, double pSum[3], long long* pCount)
{
    const uint16_t tC0 = static_cast<uint16_t>(pBlock[0] | (pBlock[1] << 8));
    const uint16_t tC1 = static_cast<uint16_t>(pBlock[2] | (pBlock[3] << 8));
    int tPalette[4][3];
    Expand565(tC0, tPalette[0]);
    Expand565(tC1, tPalette[1]);
    const bool tFour = pFourColor || tC0 > tC1;
    for (int tChannel = 0; tChannel < 3; ++tChannel)
    {
        if (tFour)
        {
            tPalette[2][tChannel] = (2 * tPalette[0][tChannel] + tPalette[1][tChannel]) / 3;
            tPalette[3][tChannel] = (tPalette[0][tChannel] + 2 * tPalette[1][tChannel]) / 3;
        }
        else
        {
            tPalette[2][tChannel] = (tPalette[0][tChannel] + tPalette[1][tChannel]) / 2;
            tPalette[3][tChannel] = 0;  // DXT1 3 renk modu: 3 = saydam (sayilmaz)
        }
    }
    const uint32_t tIndices = U32At(pBlock + 4);
    for (int tPixel = 0; tPixel < 16; ++tPixel)
    {
        const int tIndex = (tIndices >> (2 * tPixel)) & 3;
        if (!tFour && tIndex == 3)
        {
            continue;
        }
        for (int tChannel = 0; tChannel < 3; ++tChannel)
        {
            pSum[tChannel] += tPalette[tIndex][tChannel];
        }
        ++*pCount;
    }
}

// .ter dosyasindan BASE_TEX yolu (dosyanin klasorune gore); yoksa bos
std::string BaseTexture(const Fs::path& pTerPath)
{
    std::ifstream tFile(pTerPath);
    std::string tLine;
    while (std::getline(tFile, tLine))
    {
        std::istringstream tStream(tLine);
        std::string tKeyword;
        std::string tValue;
        tStream >> tKeyword >> tValue;
        if (tKeyword == "BASE_TEX" || tKeyword == "BASE_TEX_NOWRAP")
        {
            return (pTerPath.parent_path() / tValue).lexically_normal().string();
        }
    }
    return "";
}

}  // namespace

bool AverageDdsColor(const std::string& pPath, TerrainColor* pOut)
{
    std::ifstream tFile(pPath, std::ios::binary);
    uint8_t tHeader[128];
    if (!tFile.read(reinterpret_cast<char*>(tHeader), sizeof(tHeader)) || std::memcmp(tHeader, "DDS ", 4) != 0)
    {
        return false;
    }
    const int tHeight = static_cast<int>(U32At(tHeader + 12));
    const int tWidth = static_cast<int>(U32At(tHeader + 16));
    const int tMipCount = std::max(1, static_cast<int>(U32At(tHeader + 28)));
    const uint32_t tFormatFlags = U32At(tHeader + 80);
    const bool tFourCc = (tFormatFlags & 0x4) != 0;
    const std::string tCode(reinterpret_cast<const char*>(tHeader + 84), 4);
    const int tBitCount = static_cast<int>(U32At(tHeader + 88));
    int tBlockBytes = 0;  // 0: sikistirmasiz 32 bit
    if (tFourCc)
    {
        tBlockBytes = (tCode == "DXT1") ? 8 : ((tCode == "DXT3" || tCode == "DXT5") ? 16 : -1);
    }
    else if (tBitCount != 32)
    {
        return false;
    }
    if (tBlockBytes < 0 || tWidth <= 0 || tHeight <= 0)
    {
        return false;
    }
    // Mip zinciri: ilk kenari kMaxMipSide'dan kucuk seviyeyi bul, ondan oncekilerin boyutunu atla
    auto tLevelBytes = [&](int pWidth, int pHeight)
    {
        if (tBlockBytes > 0)
        {
            return static_cast<size_t>(std::max(1, (pWidth + 3) / 4)) * std::max(1, (pHeight + 3) / 4) * tBlockBytes;
        }
        return static_cast<size_t>(pWidth) * pHeight * 4;
    };
    size_t tOffset = sizeof(tHeader);
    int tLevelWidth = tWidth;
    int tLevelHeight = tHeight;
    for (int tLevel = 0; tLevel + 1 < tMipCount && std::max(tLevelWidth, tLevelHeight) > kMaxMipSide; ++tLevel)
    {
        tOffset += tLevelBytes(tLevelWidth, tLevelHeight);
        tLevelWidth = std::max(1, tLevelWidth / 2);
        tLevelHeight = std::max(1, tLevelHeight / 2);
    }
    std::vector<uint8_t> tData(tLevelBytes(tLevelWidth, tLevelHeight));
    tFile.seekg(static_cast<std::streamoff>(tOffset));
    if (!tFile.read(reinterpret_cast<char*>(tData.data()), static_cast<std::streamsize>(tData.size())))
    {
        return false;
    }
    double tSum[3] = {0.0, 0.0, 0.0};
    long long tCount = 0;
    if (tBlockBytes > 0)
    {
        const size_t tColorOffset = (tBlockBytes == 16) ? 8 : 0;  // DXT3/5: once 8 bayt alfa
        for (size_t tBlock = 0; tBlock + tBlockBytes <= tData.size(); tBlock += tBlockBytes)
        {
            AddColorBlock(tData.data() + tBlock + tColorOffset, tBlockBytes == 16, tSum, &tCount);
        }
    }
    else
    {
        for (size_t tPixel = 0; tPixel + 4 <= tData.size(); tPixel += 4)
        {
            tSum[0] += tData[tPixel + 2];  // BGRA
            tSum[1] += tData[tPixel + 1];
            tSum[2] += tData[tPixel];
            ++tCount;
        }
    }
    if (tCount == 0)
    {
        return false;
    }
    *pOut = {static_cast<uint8_t>(tSum[0] / tCount + 0.5), static_cast<uint8_t>(tSum[1] / tCount + 0.5),
             static_cast<uint8_t>(tSum[2] / tCount + 0.5)};
    return true;
}

bool LoadXPlaneClassColors(const std::string& pXPlaneDirectory, const std::string& pCachePath,
                           XPlaneClassColors* pOut, std::string* pLog)
{
    std::error_code tError;
    const Fs::path tFolder = Fs::path(pXPlaneDirectory) / kTerrainFolder;
    std::vector<Fs::path> tTerFiles;
    uintmax_t tTotalSize = 0;
    for (const Fs::directory_entry& tEntry : Fs::directory_iterator(tFolder, tError))
    {
        if (tEntry.path().extension() == ".ter")
        {
            tTerFiles.push_back(tEntry.path());
            tTotalSize += tEntry.file_size(tError);
        }
    }
    if (tTerFiles.empty())
    {
        *pLog = "terrain palette: no .ter files in " + tFolder.string() + "; built-in colors";
        return false;
    }
    const std::string tKey = std::format("xplane-palette-v1|{}|{}", tTerFiles.size(), tTotalSize);

    // Onbellek: "anahtar" satiri + sinif basina "indeks sayi R G B"
    if (!pCachePath.empty())
    {
        std::ifstream tCache(pCachePath);
        std::string tLine;
        if (std::getline(tCache, tLine) && tLine == tKey)
        {
            XPlaneClassColors tCached;
            int tClass = 0;
            int tCount = 0;
            int tR = 0;
            int tG = 0;
            int tB = 0;
            while (tCache >> tClass >> tCount >> tR >> tG >> tB)
            {
                if (tClass >= 0 && tClass < kTerrainClassCount)
                {
                    tCached.mTextureCounts[tClass] = tCount;
                    tCached.mColors[tClass] = {static_cast<uint8_t>(tR), static_cast<uint8_t>(tG),
                                               static_cast<uint8_t>(tB)};
                }
            }
            *pOut = tCached;
            *pLog = "terrain palette: X-Plane texture colors from cache";
            return true;
        }
    }

    double tSums[kTerrainClassCount][3] = {};
    XPlaneClassColors tResult;
    int tRead = 0;
    for (const Fs::path& tTer : tTerFiles)
    {
        const TerrainClass tClass = ClassifyTerrain(tTer.filename().string());
        if (tClass == TerrainClass::None || tClass == TerrainClass::Water)
        {
            continue;
        }
        const std::string tTexture = BaseTexture(tTer);
        TerrainColor tColor;
        if (tTexture.empty() || !AverageDdsColor(tTexture, &tColor))
        {
            continue;  // PNG / bilinmeyen bicim: sayilmaz
        }
        const int tIndex = static_cast<int>(tClass);
        tSums[tIndex][0] += tColor.mR;
        tSums[tIndex][1] += tColor.mG;
        tSums[tIndex][2] += tColor.mB;
        ++tResult.mTextureCounts[tIndex];
        ++tRead;
    }
    for (int tIndex = 0; tIndex < kTerrainClassCount; ++tIndex)
    {
        const int tCount = tResult.mTextureCounts[tIndex];
        if (tCount > 0)
        {
            tResult.mColors[tIndex] = {static_cast<uint8_t>(tSums[tIndex][0] / tCount + 0.5),
                                       static_cast<uint8_t>(tSums[tIndex][1] / tCount + 0.5),
                                       static_cast<uint8_t>(tSums[tIndex][2] / tCount + 0.5)};
        }
    }
    *pOut = tResult;
    *pLog = std::format("terrain palette: averaged {} X-Plane terrain textures ({} .ter files)", tRead,
                        tTerFiles.size());
    if (!pCachePath.empty())
    {
        Fs::create_directories(Fs::path(pCachePath).parent_path(), tError);
        std::ofstream tCache(pCachePath, std::ios::trunc);
        tCache << tKey << "\n";
        for (int tIndex = 0; tIndex < kTerrainClassCount; ++tIndex)
        {
            const TerrainColor& tColor = tResult.mColors[tIndex];
            tCache << tIndex << ' ' << tResult.mTextureCounts[tIndex] << ' ' << static_cast<int>(tColor.mR) << ' '
                   << static_cast<int>(tColor.mG) << ' ' << static_cast<int>(tColor.mB) << "\n";
        }
    }
    return tRead > 0;
}
