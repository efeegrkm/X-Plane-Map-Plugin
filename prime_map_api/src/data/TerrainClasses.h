// DSF arazi turu adlarini cizim siniflarina cevirir ve sinif renklerini tutar.
#ifndef PRIME_MAP_SRC_DATA_TERRAINCLASSES_H
#define PRIME_MAP_SRC_DATA_TERRAINCLASSES_H

#include <array>
#include <cstdint>
#include <string>

#include "data/TerrainNoise.h"

class Settings;

enum class TerrainClass : uint8_t
{
    None = 0,
    Water,
    Urban,
    ForestConifer,
    ForestBroadleaf,
    Shrub,
    Grass,
    Crop,
    Wetland,
    Rock,
    SnowIce,
    Tundra,
    Sand,
    Airport,
    Land,
};

const int kTerrainClassCount = 15;

TerrainClass classifyTerrain(const std::string& pTerrainName);

const char* terrainClassKey(TerrainClass pClass);

struct TerrainColor
{
    uint8_t mR;
    uint8_t mG;
    uint8_t mB;
};

enum class TerrainStyle
{
    Classes,
    Elevation,
    LandWater,
};

struct TerrainImageStyle
{
    TerrainStyle mStyle = TerrainStyle::Classes;
    TerrainColor mLand = {0, 0, 0};
    TerrainColor mWater = {0, 0, 0};
    bool mHasBuiltUp = false;
    TerrainColor mBuiltUp = {0, 0, 0};
};

TerrainColor elevationColor(double pMeters);

struct TerrainReliefParams
{
    bool mMultiDirectional = true;
    double mExaggeration = 2.5;
    double mCoarseBoost = 0.35;
    double mSlopeTone = 0.12;
    double mFlatSlope = 0.02;
};

struct TerrainWaterParams
{
    bool mCoastline = true;
    double mCoastlineStrength = 0.5;
    double mBandMeters = 200.0;
    double mBandMaxPixels = 24.0;
    double mBandStrength = 0.10;
    double mDepthTone = 0.25;
    double mDepthFullMeters = 200.0;
};

const int kVfrBandCount = 8;
struct TerrainVfrParams
{
    bool mBands = true;
    bool mContourLines = false;
    double mContourStrength = 0.35;
    std::array<double, kVfrBandCount> mBandFeet;
    std::array<TerrainColor, kVfrBandCount> mBandColors;
};

TerrainVfrParams defaultVfrParams();

class TerrainPalette
{
public:
    static const int kPaletteVersion = 3;

    TerrainPalette();

    void load(const Settings& pSettings);
    void save(Settings& pSettings) const;

    const TerrainColor& colorOf(TerrainClass pClass) const;
    double shadingStrength() const;

    double blendMeters() const;
    void setBlendMeters(double pMeters);
    double blendMaxPixels() const;
    void setBlendMaxPixels(double pPixels);

    const TerrainWarpParams& warp() const;
    void setWarp(const TerrainWarpParams& pParams);

    double closeupClassMix() const;
    void setCloseupClassMix(double pMix);

    TerrainColor referenceLandColor() const;

    double closeupMixAt(double pMetersPerPixel) const;

    std::array<TerrainColor, kTerrainClassCount> mixedColors(double pMix) const;

    bool loadedCurrentVersion() const;

    bool usesXPlaneColors() const;
    void applyDerivedColors(const std::array<TerrainColor, kTerrainClassCount>& pColors,
                            const std::array<int, kTerrainClassCount>& pCounts);

    const TerrainReliefParams& relief() const;
    void setRelief(const TerrainReliefParams& pParams);
    const TerrainWaterParams& water() const;
    void setWater(const TerrainWaterParams& pParams);
    const TerrainVfrParams& vfr() const;
    void setVfr(const TerrainVfrParams& pParams);

    bool hasBuiltUpColor() const;
    TerrainColor builtUpColor() const;
    void setBuiltUpColor(const TerrainColor& pColor);
    void clearBuiltUpColor();

private:
    TerrainColor mColors[kTerrainClassCount];
    double mShadingStrength;
    double mBlendMeters;
    double mBlendMaxPixels;
    TerrainWarpParams mWarp;
    double mCloseupClassMix;
    bool mLoadedCurrentVersion = true;
    bool mUseXPlaneColors = true;
    bool mBuiltUpOverride = false;
    TerrainColor mBuiltUp{0, 0, 0};
    TerrainReliefParams mRelief;
    TerrainWaterParams mWater;
    TerrainVfrParams mVfr;
};
#endif
