// TerrainClasses.cpp
#include "data/TerrainClasses.h"

#include <cstdio>

#include "Settings.h"

namespace
{

struct PrefixRule
{
    const char* mPrefix;
    TerrainClass mClass;
};

// Sira onemli degil (on ekler birbirinin basi degil); "sq_" once atilir
const PrefixRule kPrefixRules[] = {
    {"urban", TerrainClass::Urban},         {"coni", TerrainClass::ForestConifer},
    {"broad", TerrainClass::ForestBroadleaf}, {"rainforest", TerrainClass::ForestBroadleaf},
    {"shrb", TerrainClass::Shrub},          {"grass", TerrainClass::Grass},
    {"pastr", TerrainClass::Grass},         {"spr", TerrainClass::Grass},
    {"sp", TerrainClass::Grass},            {"crpirg", TerrainClass::Crop},
    {"crp", TerrainClass::Crop},            {"orch", TerrainClass::Crop},
    {"wetl", TerrainClass::Wetland},        {"rivbd", TerrainClass::Wetland},
    {"rock", TerrainClass::Rock},           {"bare", TerrainClass::Rock},
    {"mine", TerrainClass::Rock},           {"morn", TerrainClass::Rock},
    {"snow", TerrainClass::SnowIce},        {"ice", TerrainClass::SnowIce},
    {"tun", TerrainClass::Tundra},          {"sand", TerrainClass::Sand},
    {"semidry", TerrainClass::Sand},        {"dry", TerrainClass::Sand},
    {"apt", TerrainClass::Airport},
};

const char* kClassKeys[kTerrainClassCount] = {
    "none",  "water", "urban", "forest_conifer", "forest_broadleaf", "shrub", "grass", "crop",
    "wetland", "rock", "snow_ice", "tundra", "sand", "airport", "land",
};

// Varsayilan renkler: koyu arayuzle uyumlu, sembollerin okunabildigi soluk tonlar
const TerrainColor kDefaultColors[kTerrainClassCount] = {
    {0, 0, 0},       {40, 74, 112},   {108, 106, 110}, {44, 82, 54},   {58, 94, 56},
    {92, 100, 70},   {102, 116, 78},  {122, 118, 78},  {64, 94, 84},   {114, 108, 98},
    {214, 220, 228}, {112, 116, 100}, {156, 142, 108}, {112, 120, 100}, {100, 108, 82},
};

const double kDefaultShading = 0.45;

}  // namespace

TerrainClass ClassifyTerrain(const std::string& pTerrainName)
{
    if (pTerrainName == "terrain_Water")
    {
        return TerrainClass::Water;
    }
    // Dosya adi (son '/' sonrasi), uzanti ve "sq_" on eki olmadan
    const size_t tSlash = pTerrainName.find_last_of("/\\");
    std::string tName = (tSlash == std::string::npos) ? pTerrainName : pTerrainName.substr(tSlash + 1);
    if (tName.rfind("sq_", 0) == 0)
    {
        tName.erase(0, 3);
    }
    if (tName.find("crptwn") != std::string::npos)
    {
        return TerrainClass::Crop;  // "tarla arasi kasaba": haritada tarla gibi
    }
    const size_t tUnderscore = tName.find('_');
    const std::string tPrefix = tName.substr(0, tUnderscore == std::string::npos ? tName.find('.') : tUnderscore);
    for (const PrefixRule& tRule : kPrefixRules)
    {
        if (tPrefix == tRule.mPrefix)
        {
            return tRule.mClass;
        }
    }
    return TerrainClass::Land;
}

const char* TerrainClassKey(TerrainClass pClass)
{
    return kClassKeys[static_cast<int>(pClass)];
}

TerrainPalette::TerrainPalette()
    : mShadingStrength(kDefaultShading)
{
    for (int tIndex = 0; tIndex < kTerrainClassCount; ++tIndex)
    {
        mColors[tIndex] = kDefaultColors[tIndex];
    }
}

void TerrainPalette::Load(const Settings& pSettings)
{
    for (int tIndex = 1; tIndex < kTerrainClassCount; ++tIndex)
    {
        const std::string tValue = pSettings.GetString("colors", kClassKeys[tIndex], "");
        int tR = 0;
        int tG = 0;
        int tB = 0;
        if (std::sscanf(tValue.c_str(), "%d %d %d", &tR, &tG, &tB) == 3)
        {
            mColors[tIndex] = {static_cast<uint8_t>(tR & 255), static_cast<uint8_t>(tG & 255),
                               static_cast<uint8_t>(tB & 255)};
        }
    }
    mShadingStrength = pSettings.GetDouble("shading", "strength", kDefaultShading);
    if (mShadingStrength < 0.0)
    {
        mShadingStrength = 0.0;
    }
    if (mShadingStrength > 1.0)
    {
        mShadingStrength = 1.0;
    }
}

void TerrainPalette::Save(Settings& pSettings) const
{
    for (int tIndex = 1; tIndex < kTerrainClassCount; ++tIndex)
    {
        const TerrainColor& tColor = mColors[tIndex];
        char tValue[32];
        std::snprintf(tValue, sizeof(tValue), "%d %d %d", tColor.mR, tColor.mG, tColor.mB);
        pSettings.SetString("colors", kClassKeys[tIndex], tValue);
    }
    pSettings.SetDouble("shading", "strength", mShadingStrength);
}

const TerrainColor& TerrainPalette::ColorOf(TerrainClass pClass) const
{
    return mColors[static_cast<int>(pClass)];
}

double TerrainPalette::ShadingStrength() const
{
    return mShadingStrength;
}
