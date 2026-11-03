// TerrainRaster.cpp
#include "data/TerrainRaster.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

#include "data/DsfReader.h"

namespace
{

const char kMagic[4] = {'M', 'P', 'T', 'R'};
const uint32_t kFileVersion = 1;

const double kPi = 3.14159265358979323846;
const double kMetersPerDegreeLat = 110574.0;
const double kSunAzimuthDeg = 315.0;  // kuzeybati: kartografyadaki alisilmis isik yonu
const double kSunAltitudeDeg = 45.0;
const double kExaggeration = 2.5;     // kucuk olcekte kabartma gorunsun diye
const int kMaxSamplesPerAxis = 4;     // kucultmede piksel basina en fazla 4x4 ornek

int ClampInt(int pValue, int pLow, int pHigh)
{
    return pValue < pLow ? pLow : (pValue > pHigh ? pHigh : pValue);
}

void WriteU32(std::ofstream& pFile, uint32_t pValue)
{
    pFile.write(reinterpret_cast<const char*>(&pValue), sizeof(pValue));  // Windows/x86: kucuk-endian
}

bool ReadU32(std::ifstream& pFile, uint32_t* pValue)
{
    return static_cast<bool>(pFile.read(reinterpret_cast<char*>(pValue), sizeof(*pValue)));
}

}  // namespace

void TerrainRaster::Reset(int pWest, int pSouth)
{
    mWest = pWest;
    mSouth = pSouth;
    mClasses.assign(static_cast<size_t>(kSize) * kSize, static_cast<uint8_t>(TerrainClass::None));
    mShade.clear();
}

int TerrainRaster::West() const
{
    return mWest;
}

int TerrainRaster::South() const
{
    return mSouth;
}

void TerrainRaster::FillTriangle(const double pLon[3], const double pLat[3], TerrainClass pClass)
{
    // Piksel koordinati: x = (boylam - bati) * kSize, y = (kuzey - enlem) * kSize
    double tX[3];
    double tY[3];
    for (int tCorner = 0; tCorner < 3; ++tCorner)
    {
        tX[tCorner] = (pLon[tCorner] - mWest) * kSize;
        tY[tCorner] = (mSouth + 1 - pLat[tCorner]) * kSize;
    }
    // Isaretli alan: yonu (saat yonu / tersi) ne olursa olsun ayni testle doldurmak icin
    const double tArea = (tX[1] - tX[0]) * (tY[2] - tY[0]) - (tX[2] - tX[0]) * (tY[1] - tY[0]);
    if (std::fabs(tArea) < 1e-12)
    {
        return;
    }
    const double tSign = tArea > 0 ? 1.0 : -1.0;

    const int tMinX = ClampInt(static_cast<int>(std::floor(std::min({tX[0], tX[1], tX[2]}))), 0, kSize - 1);
    const int tMaxX = ClampInt(static_cast<int>(std::ceil(std::max({tX[0], tX[1], tX[2]}))), 0, kSize - 1);
    const int tMinY = ClampInt(static_cast<int>(std::floor(std::min({tY[0], tY[1], tY[2]}))), 0, kSize - 1);
    const int tMaxY = ClampInt(static_cast<int>(std::ceil(std::max({tY[0], tY[1], tY[2]}))), 0, kSize - 1);
    const uint8_t tValue = static_cast<uint8_t>(pClass);

    for (int tRow = tMinY; tRow <= tMaxY; ++tRow)
    {
        const double tPy = tRow + 0.5;
        for (int tColumn = tMinX; tColumn <= tMaxX; ++tColumn)
        {
            const double tPx = tColumn + 0.5;
            // Uc kenarin hepsinin ayni tarafindaysa nokta icerdedir (kenar fonksiyonlari)
            const double tE0 = ((tX[1] - tX[0]) * (tPy - tY[0]) - (tY[1] - tY[0]) * (tPx - tX[0])) * tSign;
            const double tE1 = ((tX[2] - tX[1]) * (tPy - tY[1]) - (tY[2] - tY[1]) * (tPx - tX[1])) * tSign;
            const double tE2 = ((tX[0] - tX[2]) * (tPy - tY[2]) - (tY[0] - tY[2]) * (tPx - tX[2])) * tSign;
            if (tE0 >= 0 && tE1 >= 0 && tE2 >= 0)
            {
                mClasses[static_cast<size_t>(tRow) * kSize + tColumn] = tValue;
            }
        }
    }
}

void TerrainRaster::ComputeShading(const DsfRaster& pElevation)
{
    const int tWidth = pElevation.mWidth;
    const int tHeight = pElevation.mHeight;
    if (tWidth < 3 || tHeight < 3)
    {
        return;
    }
    // DEM hucre boyu (metre); kose esasli DEM'de 1 derecede (N - 1) aralik vardir
    const double tLatMid = (mSouth + 0.5) * kPi / 180.0;
    const double tIntervalsX = pElevation.mPostCentric ? tWidth - 1 : tWidth;
    const double tIntervalsY = pElevation.mPostCentric ? tHeight - 1 : tHeight;
    const double tCellX = kMetersPerDegreeLat * std::cos(tLatMid) / tIntervalsX;
    const double tCellY = kMetersPerDegreeLat / tIntervalsY;

    const double tAzimuth = kSunAzimuthDeg * kPi / 180.0;
    const double tAltitude = kSunAltitudeDeg * kPi / 180.0;
    const double tSunX = std::sin(tAzimuth) * std::cos(tAltitude);  // dogu bileseni
    const double tSunY = std::cos(tAzimuth) * std::cos(tAltitude);  // kuzey bileseni
    const double tSunZ = std::sin(tAltitude);

    auto tElevation = [&](int pX, int pY)
    {
        return static_cast<double>(pElevation.mValues[static_cast<size_t>(ClampInt(pY, 0, tHeight - 1)) * tWidth +
                                                      ClampInt(pX, 0, tWidth - 1)]);
    };

    mShade.assign(static_cast<size_t>(kShadeSize) * kShadeSize, 0);
    for (int tRow = 0; tRow < kShadeSize; ++tRow)
    {
        // Golge raster'i satir 0 = kuzey; DEM satir 0 = guney
        const double tV = (tRow + 0.5) / kShadeSize;  // 0 = kuzey
        const int tDemY = ClampInt(static_cast<int>((1.0 - tV) * (tHeight - 1) + 0.5), 0, tHeight - 1);
        for (int tColumn = 0; tColumn < kShadeSize; ++tColumn)
        {
            const double tU = (tColumn + 0.5) / kShadeSize;
            const int tDemX = ClampInt(static_cast<int>(tU * (tWidth - 1) + 0.5), 0, tWidth - 1);
            const double tDzDx =
                (tElevation(tDemX + 1, tDemY) - tElevation(tDemX - 1, tDemY)) / (2.0 * tCellX) * kExaggeration;
            const double tDzDy =
                (tElevation(tDemX, tDemY + 1) - tElevation(tDemX, tDemY - 1)) / (2.0 * tCellY) * kExaggeration;
            // Yuzey normali (-dz/dx, -dz/dy, 1) ile gunes yonunun acisi
            const double tLength = std::sqrt(tDzDx * tDzDx + tDzDy * tDzDy + 1.0);
            double tLight = (-tDzDx * tSunX - tDzDy * tSunY + tSunZ) / tLength;
            tLight = tLight < 0.0 ? 0.0 : tLight;
            mShade[static_cast<size_t>(tRow) * kShadeSize + tColumn] = static_cast<uint8_t>(tLight * 255.0 + 0.5);
        }
    }
}

bool TerrainRaster::HasShading() const
{
    return !mShade.empty();
}

TerrainClass TerrainRaster::ClassAt(int pX, int pY) const
{
    return static_cast<TerrainClass>(mClasses[static_cast<size_t>(pY) * kSize + pX]);
}

int TerrainRaster::CountOf(TerrainClass pClass) const
{
    return static_cast<int>(std::count(mClasses.begin(), mClasses.end(), static_cast<uint8_t>(pClass)));
}

double TerrainRaster::ShadeAt(double pU, double pV) const
{
    const double tX = pU * kShadeSize - 0.5;
    const double tY = pV * kShadeSize - 0.5;
    const int tX0 = ClampInt(static_cast<int>(std::floor(tX)), 0, kShadeSize - 1);
    const int tY0 = ClampInt(static_cast<int>(std::floor(tY)), 0, kShadeSize - 1);
    const int tX1 = ClampInt(tX0 + 1, 0, kShadeSize - 1);
    const int tY1 = ClampInt(tY0 + 1, 0, kShadeSize - 1);
    const double tFx = std::min(std::max(tX - tX0, 0.0), 1.0);
    const double tFy = std::min(std::max(tY - tY0, 0.0), 1.0);
    auto tAt = [&](int pColumn, int pRow) { return mShade[static_cast<size_t>(pRow) * kShadeSize + pColumn]; };
    const double tTop = tAt(tX0, tY0) * (1.0 - tFx) + tAt(tX1, tY0) * tFx;
    const double tBottom = tAt(tX0, tY1) * (1.0 - tFx) + tAt(tX1, tY1) * tFx;
    return (tTop * (1.0 - tFy) + tBottom * tFy) / 255.0;
}

void TerrainRaster::BuildImage(int pX0, int pY0, int pX1, int pY1, int pOutSize, const TerrainPalette& pPalette,
                               std::vector<uint8_t>* pRgba) const
{
    pRgba->assign(static_cast<size_t>(pOutSize) * pOutSize * 4, 0);
    const double tBlockX = static_cast<double>(pX1 - pX0) / pOutSize;
    const double tBlockY = static_cast<double>(pY1 - pY0) / pOutSize;
    const int tSamplesX = std::max(1, std::min(kMaxSamplesPerAxis, static_cast<int>(tBlockX)));
    const int tSamplesY = std::max(1, std::min(kMaxSamplesPerAxis, static_cast<int>(tBlockY)));
    const double tStrength = pPalette.ShadingStrength();
    const double tFlatLight = std::sin(kSunAltitudeDeg * kPi / 180.0);  // duz arazinin isigi

    for (int tRow = 0; tRow < pOutSize; ++tRow)
    {
        for (int tColumn = 0; tColumn < pOutSize; ++tColumn)
        {
            double tR = 0.0;
            double tG = 0.0;
            double tB = 0.0;
            int tCovered = 0;
            int tLand = 0;
            for (int tSampleY = 0; tSampleY < tSamplesY; ++tSampleY)
            {
                const int tY = ClampInt(pY0 + static_cast<int>((tRow + (tSampleY + 0.5) / tSamplesY) * tBlockY), 0,
                                        kSize - 1);
                for (int tSampleX = 0; tSampleX < tSamplesX; ++tSampleX)
                {
                    const int tX = ClampInt(
                        pX0 + static_cast<int>((tColumn + (tSampleX + 0.5) / tSamplesX) * tBlockX), 0, kSize - 1);
                    const TerrainClass tClass = ClassAt(tX, tY);
                    if (tClass == TerrainClass::None)
                    {
                        continue;
                    }
                    const TerrainColor& tColor = pPalette.ColorOf(tClass);
                    tR += tColor.mR;
                    tG += tColor.mG;
                    tB += tColor.mB;
                    ++tCovered;
                    if (tClass != TerrainClass::Water)
                    {
                        ++tLand;
                    }
                }
            }
            if (tCovered == 0)
            {
                continue;  // saydam
            }
            double tFactor = 1.0;
            if (HasShading() && tLand > 0 && tStrength > 0.0)
            {
                // Golge: duz araziye gore aydinlik farki; sadece kara oraninda
                const double tU = (pX0 + (tColumn + 0.5) * tBlockX) / kSize;
                const double tV = (pY0 + (tRow + 0.5) * tBlockY) / kSize;
                const double tDelta = (ShadeAt(tU, tV) - tFlatLight) * 1.8 * tStrength;
                tFactor = 1.0 + tDelta * tLand / tCovered;
                tFactor = std::min(std::max(tFactor, 0.45), 1.55);
            }
            uint8_t* tPixel = pRgba->data() + (static_cast<size_t>(tRow) * pOutSize + tColumn) * 4;
            tPixel[0] = static_cast<uint8_t>(std::min(255.0, tR / tCovered * tFactor));
            tPixel[1] = static_cast<uint8_t>(std::min(255.0, tG / tCovered * tFactor));
            tPixel[2] = static_cast<uint8_t>(std::min(255.0, tB / tCovered * tFactor));
            tPixel[3] = static_cast<uint8_t>(255 * tCovered / (tSamplesX * tSamplesY));
        }
    }
}

bool TerrainRaster::Save(const std::string& pPath, const std::string& pKey) const
{
    std::ofstream tFile(pPath, std::ios::binary | std::ios::trunc);
    if (!tFile)
    {
        return false;
    }
    tFile.write(kMagic, 4);
    WriteU32(tFile, kFileVersion);
    WriteU32(tFile, static_cast<uint32_t>(pKey.size()));
    tFile.write(pKey.data(), static_cast<std::streamsize>(pKey.size()));
    WriteU32(tFile, static_cast<uint32_t>(mWest + 1000));  // eksi sayi olmasin
    WriteU32(tFile, static_cast<uint32_t>(mSouth + 1000));

    // Sinif raster'i: (tekrar sayisi uint16, deger uint8) ciftleri. Ayni turden
    // genis alanlar (deniz, orman) cok iyi sikisir.
    std::vector<uint8_t> tRuns;
    size_t tIndex = 0;
    while (tIndex < mClasses.size())
    {
        const uint8_t tValue = mClasses[tIndex];
        size_t tRun = 1;
        while (tIndex + tRun < mClasses.size() && mClasses[tIndex + tRun] == tValue && tRun < 65535)
        {
            ++tRun;
        }
        tRuns.push_back(static_cast<uint8_t>(tRun & 0xFF));
        tRuns.push_back(static_cast<uint8_t>(tRun >> 8));
        tRuns.push_back(tValue);
        tIndex += tRun;
    }
    WriteU32(tFile, static_cast<uint32_t>(tRuns.size()));
    tFile.write(reinterpret_cast<const char*>(tRuns.data()), static_cast<std::streamsize>(tRuns.size()));
    WriteU32(tFile, static_cast<uint32_t>(mShade.size()));
    tFile.write(reinterpret_cast<const char*>(mShade.data()), static_cast<std::streamsize>(mShade.size()));
    return static_cast<bool>(tFile);
}

bool TerrainRaster::Load(const std::string& pPath, const std::string& pKey)
{
    std::ifstream tFile(pPath, std::ios::binary);
    char tMagic[4] = {0, 0, 0, 0};
    uint32_t tVersion = 0;
    uint32_t tKeySize = 0;
    if (!tFile.read(tMagic, 4) || std::memcmp(tMagic, kMagic, 4) != 0 || !ReadU32(tFile, &tVersion) ||
        tVersion != kFileVersion || !ReadU32(tFile, &tKeySize) || tKeySize > (1u << 20))
    {
        return false;
    }
    std::string tKey(tKeySize, '\0');
    if (!tFile.read(&tKey[0], tKeySize) || tKey != pKey)
    {
        return false;  // kaynak DSF'ler degismis: yeniden uretilmeli
    }
    uint32_t tWest = 0;
    uint32_t tSouth = 0;
    uint32_t tRunBytes = 0;
    if (!ReadU32(tFile, &tWest) || !ReadU32(tFile, &tSouth) || !ReadU32(tFile, &tRunBytes) ||
        tRunBytes % 3 != 0)
    {
        return false;
    }
    std::vector<uint8_t> tRuns(tRunBytes);
    if (!tFile.read(reinterpret_cast<char*>(tRuns.data()), static_cast<std::streamsize>(tRunBytes)))
    {
        return false;
    }
    mWest = static_cast<int>(tWest) - 1000;
    mSouth = static_cast<int>(tSouth) - 1000;
    mClasses.clear();
    mClasses.reserve(static_cast<size_t>(kSize) * kSize);
    for (size_t tIndex = 0; tIndex + 2 < tRuns.size(); tIndex += 3)
    {
        const size_t tRun = tRuns[tIndex] | (static_cast<size_t>(tRuns[tIndex + 1]) << 8);
        if (tRuns[tIndex + 2] >= kTerrainClassCount || mClasses.size() + tRun > static_cast<size_t>(kSize) * kSize)
        {
            return false;
        }
        mClasses.insert(mClasses.end(), tRun, tRuns[tIndex + 2]);
    }
    if (mClasses.size() != static_cast<size_t>(kSize) * kSize)
    {
        return false;
    }
    uint32_t tShadeBytes = 0;
    if (!ReadU32(tFile, &tShadeBytes) ||
        (tShadeBytes != 0 && tShadeBytes != static_cast<uint32_t>(kShadeSize) * kShadeSize))
    {
        return false;
    }
    mShade.resize(tShadeBytes);
    return tShadeBytes == 0 ||
           static_cast<bool>(tFile.read(reinterpret_cast<char*>(mShade.data()), static_cast<std::streamsize>(tShadeBytes)));
}
