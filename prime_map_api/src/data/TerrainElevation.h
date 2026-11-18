// Bir karonun yukseklik izgarasi ve komsu karolarla dikissiz ornekleyici.
#ifndef PRIME_MAP_SRC_DATA_TERRAINELEVATION_H
#define PRIME_MAP_SRC_DATA_TERRAINELEVATION_H

#include <cstdint>
#include <string>
#include <vector>

struct DsfRaster;

class ElevationGrid
{
public:
    static const int kLevelCount = 5;

    bool build(const DsfRaster& pElevation, const DsfRaster* pSeaLevel);

    bool isEmpty() const;
    int size(int pLevel) const;
    float post(int pLevel, int pColumn, int pRow) const;
    double cellMeters(int pLevel) const;
    int levelFor(double pMetersPerPixel) const;
    static int nominalLevelFor(double pMetersPerPixel);

    void dropFineLevels(int pFinestKept);
    int finestLevel() const;
    bool hasLevel(int pLevel) const;

    bool hasSeaLevel() const;
    int seaLevelSize() const;
    float seaLevelCell(int pColumn, int pRow) const;

    bool save(const std::string& pPath, const std::string& pKey) const;
    bool load(const std::string& pPath, const std::string& pKey);

    size_t memoryBytes() const;

private:
    void buildPyramid();

    int mBaseSize = 0;
    int mFinestLevel = 0;
    std::vector<int16_t> mBase;
    std::vector<std::vector<float>> mUpper;
    int mSeaSize = 0;
    std::vector<int16_t> mSeaLevel;
};

struct ElevationTiles
{
    const ElevationGrid* mTiles[3][3] = {};
};

class ElevationSampler
{
public:
    ElevationSampler(const ElevationGrid* const pTiles[3][3], int pLevel);

    int level() const;
    double cellMeters() const;

    void sample(double pU, double pV, double* pHeight, double* pDhDu, double* pDhDv) const;

    bool seaLevelAt(double pU, double pV, double* pValue) const;

private:
    double postAt(int pColumn, int pRow) const;
    double postFromTiles(int pColumn, int pRow, bool* pFound) const;
    double seaCellAt(int pColumn, int pRow) const;

    const ElevationGrid* mTiles[3][3];
    int mLevel = 0;
    int mSize = 0;
};
#endif
