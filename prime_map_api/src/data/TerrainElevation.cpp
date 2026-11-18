#include "data/TerrainElevation.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

#include "data/AtomicFile.h"
#include "data/DsfReader.h"
#include "data/ElevationCodec.h"

namespace
{
const char kMagic[4] = {'M', 'P', 'E', 'L'};
const uint32_t kFileVersion = 1;
const double kMetersPerDegreeLat = 110574.0;
const int kMinSize = 5;

int16_t toInt16(double pMeters)
{
    return static_cast<int16_t>(std::clamp(std::lround(pMeters), -32000L, 32000L));
}

void writeU32(std::ofstream& pFile, uint32_t pValue)
{
    pFile.write(reinterpret_cast<const char*>(&pValue), sizeof(pValue));
}

bool readU32(std::ifstream& pFile, uint32_t* pValue)
{
    return static_cast<bool>(pFile.read(reinterpret_cast<char*>(pValue), sizeof(*pValue)));
}

void writeGrid(std::ofstream& pFile, const std::vector<int16_t>& pValues, int pSize)
{
    std::vector<uint8_t> tPacked;
    if (pSize <= 0 || !ElevationCodec::compress(pValues, pSize, pSize, &tPacked))
    {
        writeU32(pFile, 0);
        return;
    }
    writeU32(pFile, static_cast<uint32_t>(pSize));
    writeU32(pFile, static_cast<uint32_t>(tPacked.size()));
    pFile.write(reinterpret_cast<const char*>(tPacked.data()), static_cast<std::streamsize>(tPacked.size()));
}

bool readGrid(std::ifstream& pFile, std::vector<int16_t>* pValues, int* pSize)
{
    uint32_t tSize = 0;
    if (!readU32(pFile, &tSize) || tSize > 8192)
    {
        return false;
    }
    *pSize = static_cast<int>(tSize);
    pValues->clear();
    if (tSize == 0)
    {
        return true;
    }
    uint32_t tPackedBytes = 0;
    if (!readU32(pFile, &tPackedBytes) || tPackedBytes > (64u << 20))
    {
        return false;
    }
    std::vector<uint8_t> tPacked(tPackedBytes);
    if (!pFile.read(reinterpret_cast<char*>(tPacked.data()), static_cast<std::streamsize>(tPackedBytes)))
    {
        return false;
    }
    return ElevationCodec::decompress(tPacked.data(), tPacked.size(), *pSize, *pSize, pValues);
}

void cubicWeights(double pT, double pValue[4], double pSlope[4])
{
    const double tT2 = pT * pT;
    const double tT3 = tT2 * pT;
    pValue[0] = 0.5 * (-pT + 2.0 * tT2 - tT3);
    pValue[1] = 0.5 * (2.0 - 5.0 * tT2 + 3.0 * tT3);
    pValue[2] = 0.5 * (pT + 4.0 * tT2 - 3.0 * tT3);
    pValue[3] = 0.5 * (-tT2 + tT3);
    pSlope[0] = 0.5 * (-1.0 + 4.0 * pT - 3.0 * tT2);
    pSlope[1] = 0.5 * (-10.0 * pT + 9.0 * tT2);
    pSlope[2] = 0.5 * (1.0 + 8.0 * pT - 9.0 * tT2);
    pSlope[3] = 0.5 * (-2.0 * pT + 3.0 * tT2);
}
}

bool ElevationGrid::build(const DsfRaster& pElevation, const DsfRaster* pSeaLevel)
{
    mBaseSize = 0;
    mFinestLevel = 0;
    mBase.clear();
    mUpper.clear();
    mSeaSize = 0;
    mSeaLevel.clear();
    const int tWidth = pElevation.mWidth;
    const int tHeight = pElevation.mHeight;
    if (tWidth < kMinSize || tHeight < kMinSize ||
        pElevation.mValues.size() != static_cast<size_t>(tWidth) * static_cast<size_t>(tHeight))
    {
        return false;
    }
    auto tSource = [&](int pColumn, int pRowFromSouth)
    {
        return static_cast<double>(pElevation.mValues[static_cast<size_t>(pRowFromSouth) * tWidth + pColumn]);
    };

    mBaseSize = tWidth;
    mBase.resize(static_cast<size_t>(mBaseSize) * mBaseSize);
    for (int tRow = 0; tRow < mBaseSize; ++tRow)
    {
        for (int tColumn = 0; tColumn < mBaseSize; ++tColumn)
        {
            double tMeters = 0.0;
            if (pElevation.mPostCentric && tWidth == tHeight)
            {
                tMeters = tSource(tColumn, mBaseSize - 1 - tRow);
            }
            else
            {
                const double tU = static_cast<double>(tColumn) / (mBaseSize - 1);
                const double tV = 1.0 - static_cast<double>(tRow) / (mBaseSize - 1);
                const double tX = std::clamp(tU * tWidth - 0.5, 0.0, tWidth - 1.0);
                const double tY = std::clamp(tV * tHeight - 0.5, 0.0, tHeight - 1.0);
                const int tX0 = std::min(static_cast<int>(tX), tWidth - 2);
                const int tY0 = std::min(static_cast<int>(tY), tHeight - 2);
                const double tFx = tX - tX0;
                const double tFy = tY - tY0;
                const double tBottom = tSource(tX0, tY0) * (1.0 - tFx) + tSource(tX0 + 1, tY0) * tFx;
                const double tTop = tSource(tX0, tY0 + 1) * (1.0 - tFx) + tSource(tX0 + 1, tY0 + 1) * tFx;
                tMeters = tBottom * (1.0 - tFy) + tTop * tFy;
            }
            mBase[static_cast<size_t>(tRow) * mBaseSize + tColumn] = toInt16(tMeters);
        }
    }

    if (pSeaLevel && pSeaLevel->mWidth == pSeaLevel->mHeight && pSeaLevel->mWidth >= 2 &&
        pSeaLevel->mValues.size() == static_cast<size_t>(pSeaLevel->mWidth) * pSeaLevel->mHeight)
    {
        mSeaSize = pSeaLevel->mWidth;
        mSeaLevel.resize(static_cast<size_t>(mSeaSize) * mSeaSize);
        for (int tRow = 0; tRow < mSeaSize; ++tRow)
        {
            for (int tColumn = 0; tColumn < mSeaSize; ++tColumn)
            {
                mSeaLevel[static_cast<size_t>(tRow) * mSeaSize + tColumn] =
                    toInt16(pSeaLevel->mValues[static_cast<size_t>(mSeaSize - 1 - tRow) * mSeaSize + tColumn]);
            }
        }
    }
    buildPyramid();
    return true;
}

void ElevationGrid::buildPyramid()
{
    mUpper.clear();
    int tSize = mBaseSize;
    for (int tLevel = 1; tLevel < kLevelCount; ++tLevel)
    {
        if ((tSize - 1) % 2 != 0 || tSize < 2 * kMinSize)
        {
            break;
        }
        const int tNextSize = (tSize - 1) / 2 + 1;
        std::vector<float> tNext(static_cast<size_t>(tNextSize) * tNextSize);
        for (int tRow = 0; tRow < tNextSize; ++tRow)
        {
            const bool tRowEdge = (tRow == 0 || tRow == tNextSize - 1);
            for (int tColumn = 0; tColumn < tNextSize; ++tColumn)
            {
                const bool tColumnEdge = (tColumn == 0 || tColumn == tNextSize - 1);
                double tSum = 0.0;
                double tWeight = 0.0;
                for (int tDy = -1; tDy <= 1; ++tDy)
                {
                    if (tRowEdge && tDy != 0)
                    {
                        continue;
                    }
                    const double tWy = (tDy == 0) ? 2.0 : 1.0;
                    for (int tDx = -1; tDx <= 1; ++tDx)
                    {
                        if (tColumnEdge && tDx != 0)
                        {
                            continue;
                        }
                        const double tWx = (tDx == 0) ? 2.0 : 1.0;
                        tSum += tWx * tWy * post(tLevel - 1, 2 * tColumn + tDx, 2 * tRow + tDy);
                        tWeight += tWx * tWy;
                    }
                }
                tNext[static_cast<size_t>(tRow) * tNextSize + tColumn] = static_cast<float>(tSum / tWeight);
            }
        }
        mUpper.push_back(std::move(tNext));
        tSize = tNextSize;
    }
}

bool ElevationGrid::isEmpty() const
{
    return mBaseSize == 0;
}

int ElevationGrid::size(int pLevel) const
{
    int tSize = mBaseSize;
    for (int tLevel = 0; tLevel < pLevel; ++tLevel)
    {
        tSize = (tSize - 1) / 2 + 1;
    }
    return tSize;
}

float ElevationGrid::post(int pLevel, int pColumn, int pRow) const
{
    if (pLevel <= 0)
    {
        return static_cast<float>(mBase[static_cast<size_t>(pRow) * mBaseSize + pColumn]);
    }
    const int tSize = size(pLevel);
    return mUpper[static_cast<size_t>(pLevel - 1)][static_cast<size_t>(pRow) * tSize + pColumn];
}

double ElevationGrid::cellMeters(int pLevel) const
{
    const int tSize = size(pLevel);
    return tSize > 1 ? kMetersPerDegreeLat / (tSize - 1) : kMetersPerDegreeLat;
}

int ElevationGrid::levelFor(double pMetersPerPixel) const
{
    const int tLevels = 1 + static_cast<int>(mUpper.size());
    for (int tLevel = tLevels - 1; tLevel > mFinestLevel; --tLevel)
    {
        if (cellMeters(tLevel) <= pMetersPerPixel)
        {
            return tLevel;
        }
    }
    return mFinestLevel;
}

int ElevationGrid::nominalLevelFor(double pMetersPerPixel)
{
    int tLevel = 0;
    double tCell = kMetersPerDegreeLat / 1200.0;
    while (tLevel + 1 < kLevelCount && tCell * 2.0 <= pMetersPerPixel)
    {
        tCell *= 2.0;
        ++tLevel;
    }
    return tLevel;
}

void ElevationGrid::dropFineLevels(int pFinestKept)
{
    const int tLevels = 1 + static_cast<int>(mUpper.size());
    const int tKeep = std::min(pFinestKept, tLevels - 1);
    if (mBase.empty() || tKeep <= mFinestLevel)
    {
        return;
    }
    std::vector<int16_t>().swap(mBase);
    for (int tLevel = 1; tLevel < tKeep; ++tLevel)
    {
        std::vector<float>().swap(mUpper[static_cast<size_t>(tLevel - 1)]);
    }
    mFinestLevel = tKeep;
}

int ElevationGrid::finestLevel() const
{
    return mFinestLevel;
}

bool ElevationGrid::hasLevel(int pLevel) const
{
    return mBaseSize > 0 && pLevel >= mFinestLevel && pLevel <= static_cast<int>(mUpper.size());
}

bool ElevationGrid::hasSeaLevel() const
{
    return !mSeaLevel.empty();
}

int ElevationGrid::seaLevelSize() const
{
    return mSeaSize;
}

float ElevationGrid::seaLevelCell(int pColumn, int pRow) const
{
    return static_cast<float>(mSeaLevel[static_cast<size_t>(pRow) * mSeaSize + pColumn]);
}

bool ElevationGrid::save(const std::string& pPath, const std::string& pKey) const
{
    if (mFinestLevel > 0)
    {
        return false;
    }
    return writeFileAtomic(pPath,
                           [this, &pKey](std::ofstream& tFile)
                           {
                               tFile.write(kMagic, 4);
                               writeU32(tFile, kFileVersion);
                               writeU32(tFile, static_cast<uint32_t>(pKey.size()));
                               tFile.write(pKey.data(), static_cast<std::streamsize>(pKey.size()));
                               writeGrid(tFile, mBase, mBaseSize);
                               writeGrid(tFile, mSeaLevel, mSeaSize);
                           });
}

bool ElevationGrid::load(const std::string& pPath, const std::string& pKey)
{
    std::ifstream tFile(pPath, std::ios::binary);
    char tMagic[4] = {0, 0, 0, 0};
    uint32_t tVersion = 0;
    uint32_t tKeySize = 0;
    if (!tFile.read(tMagic, 4) || std::memcmp(tMagic, kMagic, 4) != 0 || !readU32(tFile, &tVersion) ||
        tVersion != kFileVersion || !readU32(tFile, &tKeySize) || tKeySize > (1u << 20))
    {
        return false;
    }
    std::string tKey(tKeySize, '\0');
    if (!tFile.read(&tKey[0], tKeySize) || tKey != pKey)
    {
        return false;
    }
    int tBaseSize = 0;
    int tSeaSize = 0;
    std::vector<int16_t> tBase;
    std::vector<int16_t> tSea;
    if (!readGrid(tFile, &tBase, &tBaseSize) || !readGrid(tFile, &tSea, &tSeaSize) ||
        (tBaseSize != 0 && tBaseSize < kMinSize))
    {
        return false;
    }
    mBaseSize = tBaseSize;
    mFinestLevel = 0;
    mBase.swap(tBase);
    mSeaSize = tSeaSize;
    mSeaLevel.swap(tSea);
    buildPyramid();
    return true;
}

size_t ElevationGrid::memoryBytes() const
{
    size_t tBytes = mBase.size() * sizeof(int16_t) + mSeaLevel.size() * sizeof(int16_t);
    for (const std::vector<float>& tLevel : mUpper)
    {
        tBytes += tLevel.size() * sizeof(float);
    }
    return tBytes;
}

ElevationSampler::ElevationSampler(const ElevationGrid* const pTiles[3][3], int pLevel)
{
    for (int tRow = 0; tRow < 3; ++tRow)
    {
        for (int tColumn = 0; tColumn < 3; ++tColumn)
        {
            const ElevationGrid* tTile = pTiles[tRow][tColumn];
            mTiles[tRow][tColumn] = (tTile && !tTile->isEmpty()) ? tTile : nullptr;
        }
    }
    mLevel = pLevel;
    mSize = mTiles[1][1] ? mTiles[1][1]->size(pLevel) : 0;
    for (int tRow = 0; tRow < 3; ++tRow)
    {
        for (int tColumn = 0; tColumn < 3; ++tColumn)
        {
            const ElevationGrid* tTile = mTiles[tRow][tColumn];
            if (tTile && (tTile->size(pLevel) != mSize || !tTile->hasLevel(pLevel)))
            {
                mTiles[tRow][tColumn] = nullptr;
            }
        }
    }
}

int ElevationSampler::level() const
{
    return mLevel;
}

double ElevationSampler::cellMeters() const
{
    return mTiles[1][1] ? mTiles[1][1]->cellMeters(mLevel) : kMetersPerDegreeLat;
}

double ElevationSampler::postFromTiles(int pColumn, int pRow, bool* pFound) const
{
    const int tSpan = mSize - 1;
    double tSum = 0.0;
    int tCount = 0;
    for (int tTileY = -1; tTileY <= 1; ++tTileY)
    {
        const int tLocalRow = pRow - tTileY * tSpan;
        if (tLocalRow < 0 || tLocalRow > tSpan)
        {
            continue;
        }
        for (int tTileX = -1; tTileX <= 1; ++tTileX)
        {
            const int tLocalColumn = pColumn - tTileX * tSpan;
            const ElevationGrid* tTile = mTiles[tTileY + 1][tTileX + 1];
            if (tLocalColumn < 0 || tLocalColumn > tSpan || !tTile)
            {
                continue;
            }
            tSum += tTile->post(mLevel, tLocalColumn, tLocalRow);
            ++tCount;
        }
    }
    *pFound = tCount > 0;
    return tCount > 0 ? tSum / tCount : 0.0;
}

double ElevationSampler::postAt(int pColumn, int pRow) const
{
    const int tSpan = mSize - 1;
    if (pColumn > 0 && pColumn < tSpan && pRow > 0 && pRow < tSpan)
    {
        return mTiles[1][1]->post(mLevel, pColumn, pRow);
    }
    bool tFound = false;
    const double tValue = postFromTiles(pColumn, pRow, &tFound);
    if (tFound)
    {
        return tValue;
    }
    if (pColumn < 0)
    {
        return 2.0 * postAt(0, pRow) - postAt(-pColumn, pRow);
    }
    if (pColumn > tSpan)
    {
        return 2.0 * postAt(tSpan, pRow) - postAt(2 * tSpan - pColumn, pRow);
    }
    if (pRow < 0)
    {
        return 2.0 * postAt(pColumn, 0) - postAt(pColumn, -pRow);
    }
    return 2.0 * postAt(pColumn, tSpan) - postAt(pColumn, 2 * tSpan - pRow);
}

void ElevationSampler::sample(double pU, double pV, double* pHeight, double* pDhDu, double* pDhDv) const
{
    if (!mTiles[1][1] || mSize < 2)
    {
        *pHeight = 0.0;
        *pDhDu = 0.0;
        *pDhDv = 0.0;
        return;
    }
    const int tSpan = mSize - 1;
    const double tX = pU * tSpan;
    const double tY = pV * tSpan;
    const int tBaseX = static_cast<int>(std::floor(tX));
    const int tBaseY = static_cast<int>(std::floor(tY));
    double tWx[4];
    double tDx[4];
    double tWy[4];
    double tDy[4];
    cubicWeights(tX - tBaseX, tWx, tDx);
    cubicWeights(tY - tBaseY, tWy, tDy);

    double tHeight = 0.0;
    double tSlopeX = 0.0;
    double tSlopeY = 0.0;
    for (int tRow = 0; tRow < 4; ++tRow)
    {
        double tRowValue = 0.0;
        double tRowSlope = 0.0;
        for (int tColumn = 0; tColumn < 4; ++tColumn)
        {
            const double tPost = postAt(tBaseX - 1 + tColumn, tBaseY - 1 + tRow);
            tRowValue += tWx[tColumn] * tPost;
            tRowSlope += tDx[tColumn] * tPost;
        }
        tHeight += tWy[tRow] * tRowValue;
        tSlopeX += tWy[tRow] * tRowSlope;
        tSlopeY += tDy[tRow] * tRowValue;
    }
    *pHeight = tHeight;
    *pDhDu = tSlopeX * tSpan;
    *pDhDv = tSlopeY * tSpan;
}

double ElevationSampler::seaCellAt(int pColumn, int pRow) const
{
    const ElevationGrid* tCenter = mTiles[1][1];
    const int tSize = tCenter->seaLevelSize();
    const int tTileX = pColumn < 0 ? -1 : (pColumn >= tSize ? 1 : 0);
    const int tTileY = pRow < 0 ? -1 : (pRow >= tSize ? 1 : 0);
    const ElevationGrid* tTile = mTiles[tTileY + 1][tTileX + 1];
    if (tTile && tTile->hasSeaLevel() && tTile->seaLevelSize() == tSize)
    {
        return tTile->seaLevelCell(pColumn - tTileX * tSize, pRow - tTileY * tSize);
    }
    return tCenter->seaLevelCell(std::clamp(pColumn, 0, tSize - 1), std::clamp(pRow, 0, tSize - 1));
}

bool ElevationSampler::seaLevelAt(double pU, double pV, double* pValue) const
{
    const ElevationGrid* tCenter = mTiles[1][1];
    if (!tCenter || !tCenter->hasSeaLevel())
    {
        return false;
    }
    const int tSize = tCenter->seaLevelSize();
    const double tX = pU * tSize - 0.5;
    const double tY = pV * tSize - 0.5;
    const int tBaseX = static_cast<int>(std::floor(tX));
    const int tBaseY = static_cast<int>(std::floor(tY));
    double tWx[4];
    double tWy[4];
    double tUnused[4];
    cubicWeights(tX - tBaseX, tWx, tUnused);
    cubicWeights(tY - tBaseY, tWy, tUnused);
    double tValue = 0.0;
    for (int tRow = 0; tRow < 4; ++tRow)
    {
        double tRowValue = 0.0;
        for (int tColumn = 0; tColumn < 4; ++tColumn)
        {
            tRowValue += tWx[tColumn] * seaCellAt(tBaseX - 1 + tColumn, tBaseY - 1 + tRow);
        }
        tValue += tWy[tRow] * tRowValue;
    }
    *pValue = tValue;
    return true;
}
