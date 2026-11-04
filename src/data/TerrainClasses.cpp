// TerrainClasses.cpp
#include "data/TerrainClasses.h"

#include <array>
#include <format>

#include "Settings.h"

namespace
{

struct PrefixRule
{
    const char* mPrefix;
    TerrainClass mClass;
};

// Sira onemli degil (on ekler birbirinin basi degil); "sq_" once atilir
const auto kPrefixRules = std::to_array<PrefixRule>({
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
});

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
        pSettings.SetString("colors", kClassKeys[tIndex], std::format("{} {} {}", tColor.mR, tColor.mG, tColor.mB));
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

TerrainColor ElevationColor(double pMeters)
{
    // (feet, R, G, B): sectional'daki rakim tonlarina benzer; aralar dogrusal karisir
    struct Stop
    {
        double mFeet;
        double mR;
        double mG;
        double mB;
    };
    static const Stop kStops[] = {
        {0.0, 190.0, 216.0, 166.0},    {1000.0, 210.0, 226.0, 172.0}, {2000.0, 230.0, 232.0, 180.0},
        {3000.0, 238.0, 226.0, 170.0}, {5000.0, 230.0, 206.0, 150.0}, {7000.0, 214.0, 184.0, 134.0},
        {9000.0, 196.0, 162.0, 118.0}, {12000.0, 176.0, 142.0, 106.0}, {15000.0, 160.0, 132.0, 102.0},
    };
    const int tCount = static_cast<int>(sizeof(kStops) / sizeof(kStops[0]));
    const double tFeet = pMeters * 3.28084;
    if (tFeet <= kStops[0].mFeet)
    {
        return {static_cast<uint8_t>(kStops[0].mR), static_cast<uint8_t>(kStops[0].mG),
                static_cast<uint8_t>(kStops[0].mB)};
    }
    for (int tIndex = 1; tIndex < tCount; ++tIndex)
    {
        if (tFeet <= kStops[tIndex].mFeet)
        {
            const Stop& tLow = kStops[tIndex - 1];
            const Stop& tHigh = kStops[tIndex];
            const double tF = (tFeet - tLow.mFeet) / (tHigh.mFeet - tLow.mFeet);
            return {static_cast<uint8_t>(tLow.mR + (tHigh.mR - tLow.mR) * tF + 0.5),
                    static_cast<uint8_t>(tLow.mG + (tHigh.mG - tLow.mG) * tF + 0.5),
                    static_cast<uint8_t>(tLow.mB + (tHigh.mB - tLow.mB) * tF + 0.5)};
        }
    }
    const Stop& tLast = kStops[tCount - 1];
    return {static_cast<uint8_t>(tLast.mR), static_cast<uint8_t>(tLast.mG), static_cast<uint8_t>(tLast.mB)};
}
