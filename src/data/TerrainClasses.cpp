// TerrainClasses.cpp
#include "data/TerrainClasses.h"

#include <algorithm>
#include <array>
#include <cmath>
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

// Varsayilan renkler: koyu arayuzle uyumlu, sembollerin okunabildigi soluk tonlar.
// v2: bitki ortusu ailesinin parlaklik araligi daraltildi (ucgen mozayigi daha az
// sert gorunsun); su ve kar/buz ayni. X-Plane'de bakildiktan sonra ini ile ince ay.
const TerrainColor kDefaultColors[kTerrainClassCount] = {
    {0, 0, 0},       {40, 74, 112},   {104, 104, 98},  {52, 86, 58},    {62, 96, 60},
    {80, 98, 66},    {86, 106, 70},   {98, 108, 72},   {66, 94, 80},    {108, 106, 96},
    {214, 220, 228}, {96, 104, 86},   {132, 124, 98},  {104, 108, 96},  {86, 100, 72},
};

const double kDefaultShading = 0.45;
const double kDefaultBlendMeters = 150.0;  // Scenery: siniflar arasi yumusak gecis (yari genislik, metre)
const double kMaxBlendMeters = 1000.0;
const double kDefaultBlendMaxPixels = 5.0;  // yakin zoomda erim tavani (doku pikseli); 0 = tavan yok
const double kMaxBlendMaxPixels = 64.0;
const double kDefaultWarpMeters = 65.0;      // sinir bukmesi: en buyuk kayma (0 = duz kenarlar)
const double kMaxWarpMeters = 300.0;
const double kDefaultWarpWavelength = 250.0;  // en kaba oktavin dalga boyu (metre)
const double kMinWarpWavelength = 50.0;
const double kMaxWarpWavelength = 2000.0;
const double kDefaultCloseupClassMix = 0.30;  // yakin zoomda sinif renklerinin kara tona karismasi
const double kMaxCloseupClassMix = 0.8;

// VFR bantlarinin varsayilanlari: sectional sinirlari, renkler ElevationColor duraklarindan
const double kDefaultBandFeet[kVfrBandCount] = {0.0, 1000.0, 2000.0, 3000.0, 5000.0, 7000.0, 9000.0, 12000.0};
const TerrainColor kDefaultBandColors[kVfrBandCount] = {
    {190, 216, 166}, {210, 226, 172}, {230, 232, 180}, {238, 226, 170},
    {230, 206, 150}, {214, 184, 134}, {196, 162, 118}, {176, 142, 106},
};

// "<feet> R G B" -> bant; bicim yanlissa false (varsayilan kalir)
bool ParseBand(const std::string& pText, double* pFeet, TerrainColor* pColor)
{
    double tFeet = 0.0;
    int tR = 0;
    int tG = 0;
    int tB = 0;
    if (std::sscanf(pText.c_str(), "%lf %d %d %d", &tFeet, &tR, &tG, &tB) != 4)
    {
        return false;
    }
    *pFeet = tFeet;
    *pColor = {static_cast<uint8_t>(std::clamp(tR, 0, 255)), static_cast<uint8_t>(std::clamp(tG, 0, 255)),
               static_cast<uint8_t>(std::clamp(tB, 0, 255))};
    return true;
}

}  // namespace

TerrainVfrParams DefaultVfrParams()
{
    TerrainVfrParams tParams;
    for (int tBand = 0; tBand < kVfrBandCount; ++tBand)
    {
        tParams.mBandFeet[tBand] = kDefaultBandFeet[tBand];
        tParams.mBandColors[tBand] = kDefaultBandColors[tBand];
    }
    return tParams;
}

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
    : mShadingStrength(kDefaultShading),
      mBlendMeters(kDefaultBlendMeters),
      mBlendMaxPixels(kDefaultBlendMaxPixels),
      mCloseupClassMix(kDefaultCloseupClassMix),
      mVfr(DefaultVfrParams())
{
    for (int tIndex = 0; tIndex < kTerrainClassCount; ++tIndex)
    {
        mColors[tIndex] = kDefaultColors[tIndex];
    }
}

void TerrainPalette::Load(const Settings& pSettings)
{
    // Eski surum dosyadaki renkler eski varsayilanlardir: yenilerini ezmesin
    mLoadedCurrentVersion = pSettings.GetInt("palette", "version", 1) >= kPaletteVersion;
    mUseXPlaneColors = !mLoadedCurrentVersion || pSettings.GetString("palette", "source", "xplane") != "ini";
    if (mLoadedCurrentVersion && !mUseXPlaneColors)
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
    mBlendMeters = std::clamp(pSettings.GetDouble("blend", "meters", kDefaultBlendMeters), 0.0, kMaxBlendMeters);
    // max_pixels / warp / closeup renk surumune bagli degil: her zaman okunur
    mBlendMaxPixels = std::clamp(pSettings.GetDouble("blend", "max_pixels", kDefaultBlendMaxPixels), 0.0,
                                 kMaxBlendMaxPixels);
    mWarp.mAmplitudeMeters = std::clamp(pSettings.GetDouble("warp", "meters", kDefaultWarpMeters), 0.0,
                                        kMaxWarpMeters);
    mWarp.mWavelengthMeters = std::clamp(pSettings.GetDouble("warp", "wavelength", kDefaultWarpWavelength),
                                         kMinWarpWavelength, kMaxWarpWavelength);
    mWarp.mOctaves = 3;
    mCloseupClassMix = std::clamp(pSettings.GetDouble("closeup", "class_mix", kDefaultCloseupClassMix), 0.0,
                                  kMaxCloseupClassMix);

    // 0.16: [relief], [water], [vfr]. Yoksa varsayilan; araliklar sinirlanir.
    const TerrainReliefParams tRelief;
    mRelief.mMultiDirectional = pSettings.GetBool("relief", "multi_directional", tRelief.mMultiDirectional);
    mRelief.mExaggeration = std::clamp(pSettings.GetDouble("relief", "exaggeration", tRelief.mExaggeration), 0.0, 20.0);
    mRelief.mCoarseBoost = std::clamp(pSettings.GetDouble("relief", "coarse_boost", tRelief.mCoarseBoost), 0.0, 1.0);
    mRelief.mSlopeTone = std::clamp(pSettings.GetDouble("relief", "slope_tone", tRelief.mSlopeTone), 0.0, 0.8);
    mRelief.mFlatSlope = std::clamp(pSettings.GetDouble("relief", "flat_slope", tRelief.mFlatSlope), 0.0, 0.2);

    const TerrainWaterParams tWater;
    mWater.mCoastline = pSettings.GetBool("water", "coastline", tWater.mCoastline);
    mWater.mCoastlineStrength =
        std::clamp(pSettings.GetDouble("water", "coastline_strength", tWater.mCoastlineStrength), 0.0, 1.0);
    mWater.mBandMeters = std::clamp(pSettings.GetDouble("water", "band_meters", tWater.mBandMeters), 0.0, 5000.0);
    mWater.mBandMaxPixels =
        std::clamp(pSettings.GetDouble("water", "band_max_pixels", tWater.mBandMaxPixels), 0.0, 64.0);
    mWater.mBandStrength = std::clamp(pSettings.GetDouble("water", "band_strength", tWater.mBandStrength), 0.0, 1.0);
    mWater.mDepthTone = std::clamp(pSettings.GetDouble("water", "depth_tone", tWater.mDepthTone), 0.0, 1.0);
    mWater.mDepthFullMeters =
        std::clamp(pSettings.GetDouble("water", "depth_full_meters", tWater.mDepthFullMeters), 20.0, 5000.0);

    mVfr = DefaultVfrParams();
    mVfr.mBands = pSettings.GetBool("vfr", "bands", mVfr.mBands);
    mVfr.mContourLines = pSettings.GetBool("vfr", "contour_lines", mVfr.mContourLines);
    mVfr.mContourStrength = std::clamp(pSettings.GetDouble("vfr", "contour_strength", mVfr.mContourStrength), 0.0, 1.0);
    // band_1 .. band_8 = "<alt sinir feet> R G B"; sinirlar artan olmali, degilse hepsi varsayilana doner
    TerrainVfrParams tRead = mVfr;
    bool tIncreasing = true;
    for (int tBand = 0; tBand < kVfrBandCount; ++tBand)
    {
        const std::string tText = pSettings.GetString("vfr", std::format("band_{}", tBand + 1), "");
        ParseBand(tText, &tRead.mBandFeet[tBand], &tRead.mBandColors[tBand]);
        if (tBand > 0 && tRead.mBandFeet[tBand] <= tRead.mBandFeet[tBand - 1])
        {
            tIncreasing = false;
        }
    }
    if (tIncreasing)
    {
        mVfr.mBandFeet = tRead.mBandFeet;
        mVfr.mBandColors = tRead.mBandColors;
    }
}

void TerrainPalette::Save(Settings& pSettings) const
{
    pSettings.SetInt("palette", "version", kPaletteVersion);
    pSettings.SetString("palette", "source", mUseXPlaneColors ? "xplane" : "ini");
    for (int tIndex = 1; tIndex < kTerrainClassCount; ++tIndex)
    {
        const TerrainColor& tColor = mColors[tIndex];
        pSettings.SetString("colors", kClassKeys[tIndex], std::format("{} {} {}", tColor.mR, tColor.mG, tColor.mB));
    }
    pSettings.SetDouble("shading", "strength", mShadingStrength);
    pSettings.SetDouble("blend", "meters", mBlendMeters);
    pSettings.SetDouble("blend", "max_pixels", mBlendMaxPixels);
    pSettings.SetDouble("warp", "meters", mWarp.mAmplitudeMeters);
    pSettings.SetDouble("warp", "wavelength", mWarp.mWavelengthMeters);
    pSettings.SetDouble("closeup", "class_mix", mCloseupClassMix);

    pSettings.SetBool("relief", "multi_directional", mRelief.mMultiDirectional);
    pSettings.SetDouble("relief", "exaggeration", mRelief.mExaggeration);
    pSettings.SetDouble("relief", "coarse_boost", mRelief.mCoarseBoost);
    pSettings.SetDouble("relief", "slope_tone", mRelief.mSlopeTone);
    pSettings.SetDouble("relief", "flat_slope", mRelief.mFlatSlope);

    pSettings.SetBool("water", "coastline", mWater.mCoastline);
    pSettings.SetDouble("water", "coastline_strength", mWater.mCoastlineStrength);
    pSettings.SetDouble("water", "band_meters", mWater.mBandMeters);
    pSettings.SetDouble("water", "band_max_pixels", mWater.mBandMaxPixels);
    pSettings.SetDouble("water", "band_strength", mWater.mBandStrength);
    pSettings.SetDouble("water", "depth_tone", mWater.mDepthTone);
    pSettings.SetDouble("water", "depth_full_meters", mWater.mDepthFullMeters);

    pSettings.SetBool("vfr", "bands", mVfr.mBands);
    pSettings.SetBool("vfr", "contour_lines", mVfr.mContourLines);
    pSettings.SetDouble("vfr", "contour_strength", mVfr.mContourStrength);
    for (int tBand = 0; tBand < kVfrBandCount; ++tBand)
    {
        const TerrainColor& tColor = mVfr.mBandColors[tBand];
        pSettings.SetString("vfr", std::format("band_{}", tBand + 1),
                            std::format("{:.0f} {} {} {}", mVfr.mBandFeet[tBand], tColor.mR, tColor.mG, tColor.mB));
    }
}

const TerrainColor& TerrainPalette::ColorOf(TerrainClass pClass) const
{
    return mColors[static_cast<int>(pClass)];
}

double TerrainPalette::ShadingStrength() const
{
    return mShadingStrength;
}

double TerrainPalette::BlendMeters() const
{
    return mBlendMeters;
}

void TerrainPalette::SetBlendMeters(double pMeters)
{
    mBlendMeters = std::clamp(pMeters, 0.0, kMaxBlendMeters);
}

double TerrainPalette::BlendMaxPixels() const
{
    return mBlendMaxPixels;
}

void TerrainPalette::SetBlendMaxPixels(double pPixels)
{
    mBlendMaxPixels = std::clamp(pPixels, 0.0, kMaxBlendMaxPixels);
}

const TerrainWarpParams& TerrainPalette::Warp() const
{
    return mWarp;
}

void TerrainPalette::SetWarp(const TerrainWarpParams& pParams)
{
    mWarp = pParams;
    mWarp.mAmplitudeMeters = std::clamp(pParams.mAmplitudeMeters, 0.0, kMaxWarpMeters);
    mWarp.mWavelengthMeters = std::clamp(pParams.mWavelengthMeters, kMinWarpWavelength, kMaxWarpWavelength);
    mWarp.mOctaves = pParams.mOctaves;
}

double TerrainPalette::CloseupClassMix() const
{
    return mCloseupClassMix;
}

void TerrainPalette::SetCloseupClassMix(double pMix)
{
    mCloseupClassMix = std::clamp(pMix, 0.0, kMaxCloseupClassMix);
}

TerrainColor TerrainPalette::ReferenceLandColor() const
{
    // Bitki ortusu siniflarinin aritmetik ortalamasi (karisimin referans kara tonu)
    const TerrainClass tClasses[] = {TerrainClass::ForestConifer, TerrainClass::ForestBroadleaf, TerrainClass::Shrub,
                                     TerrainClass::Grass, TerrainClass::Crop, TerrainClass::Land};
    int tR = 0;
    int tG = 0;
    int tB = 0;
    const int tCount = static_cast<int>(sizeof(tClasses) / sizeof(tClasses[0]));
    for (int tIndex = 0; tIndex < tCount; ++tIndex)
    {
        const TerrainColor& tColor = ColorOf(tClasses[tIndex]);
        tR += tColor.mR;
        tG += tColor.mG;
        tB += tColor.mB;
    }
    return {static_cast<uint8_t>(tR / tCount), static_cast<uint8_t>(tG / tCount), static_cast<uint8_t>(tB / tCount)};
}

double TerrainPalette::CloseupMixAt(double pMetersPerPixel) const
{
    // Piksel boyu 50 m ve ustu -> 0, 5 m ve alti -> tam oran; arasi logaritmik
    if (mCloseupClassMix <= 0.0 || pMetersPerPixel <= 0.0)
    {
        return 0.0;
    }
    const double tT = (std::log(50.0) - std::log(pMetersPerPixel)) / (std::log(50.0) - std::log(5.0));
    return mCloseupClassMix * std::clamp(tT, 0.0, 1.0);
}

std::array<TerrainColor, kTerrainClassCount> TerrainPalette::MixedColors(double pMix) const
{
    std::array<TerrainColor, kTerrainClassCount> tColors;
    for (int tIndex = 0; tIndex < kTerrainClassCount; ++tIndex)
    {
        tColors[tIndex] = mColors[tIndex];
    }
    if (pMix <= 0.0)
    {
        return tColors;
    }
    const TerrainColor tReference = ReferenceLandColor();
    for (int tIndex = 0; tIndex < kTerrainClassCount; ++tIndex)
    {
        const TerrainClass tClass = static_cast<TerrainClass>(tIndex);
        // Su kiyiyi, kar/buz kimligi, None bosluk: karismaz
        if (tClass == TerrainClass::None || tClass == TerrainClass::Water || tClass == TerrainClass::SnowIce)
        {
            continue;
        }
        TerrainColor& tColor = tColors[tIndex];
        tColor.mR = static_cast<uint8_t>(std::lround(tColor.mR + (tReference.mR - tColor.mR) * pMix));
        tColor.mG = static_cast<uint8_t>(std::lround(tColor.mG + (tReference.mG - tColor.mG) * pMix));
        tColor.mB = static_cast<uint8_t>(std::lround(tColor.mB + (tReference.mB - tColor.mB) * pMix));
    }
    return tColors;
}

bool TerrainPalette::LoadedCurrentVersion() const
{
    return mLoadedCurrentVersion;
}

bool TerrainPalette::UsesXPlaneColors() const
{
    return mUseXPlaneColors;
}

void TerrainPalette::ApplyDerivedColors(const std::array<TerrainColor, kTerrainClassCount>& pColors,
                                        const std::array<int, kTerrainClassCount>& pCounts)
{
    for (int tIndex = 1; tIndex < kTerrainClassCount; ++tIndex)
    {
        if (pCounts[tIndex] > 0 && static_cast<TerrainClass>(tIndex) != TerrainClass::Water)
        {
            mColors[tIndex] = pColors[tIndex];
        }
    }
}

const TerrainReliefParams& TerrainPalette::Relief() const
{
    return mRelief;
}

void TerrainPalette::SetRelief(const TerrainReliefParams& pParams)
{
    mRelief = pParams;
}

const TerrainWaterParams& TerrainPalette::Water() const
{
    return mWater;
}

void TerrainPalette::SetWater(const TerrainWaterParams& pParams)
{
    mWater = pParams;
}

const TerrainVfrParams& TerrainPalette::Vfr() const
{
    return mVfr;
}

void TerrainPalette::SetVfr(const TerrainVfrParams& pParams)
{
    mVfr = pParams;
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
