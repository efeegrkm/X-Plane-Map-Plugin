// MapTheme.cpp
#include "MapTheme.h"

namespace
{

// Ortak acik tema parcalari (VFR / IFR): beyaz kontur, koyu yazi
const Rgba kWhiteHalo = {1.0f, 1.0f, 1.0f, 0.85f};

const MapTheme kScenery = {
    MapMode::Scenery,
    "scenery",
    "Scenery",
    {0.13f, 0.15f, 0.18f, 1.0f},  // arka plan
    {0.0f, 0.0f, 0.0f, 0.0f},     // su: arazi paleti kullanilir
    TerrainStyle::Classes,
    {0.0f, 0.0f, 0.0f, 0.70f},  // koyu kontur
    false,
    {0.78f, 0.84f, 0.92f, 0.22f},
    {0.70f, 0.76f, 0.84f, 1.0f},
    {{{0.40f, 0.72f, 1.00f, 1.0f},
      {0.55f, 0.65f, 0.78f, 1.0f},
      {0.35f, 0.62f, 0.95f, 1.0f},
      {0.72f, 0.62f, 0.92f, 1.0f},
      {0.50f, 0.52f, 0.56f, 1.0f}}},
    {{{0.80f, 0.90f, 1.00f, 1.0f},
      {0.66f, 0.73f, 0.82f, 1.0f},
      {0.60f, 0.75f, 0.95f, 1.0f},
      {0.78f, 0.72f, 0.92f, 1.0f},
      {0.58f, 0.60f, 0.64f, 1.0f}}},
    false,
    {},
    {},
    false,
    {{{0.30f, 0.85f, 0.75f, 1.0f}, {0.80f, 0.60f, 1.00f, 1.0f}, {1.00f, 0.62f, 0.30f, 1.0f},
      {0.72f, 0.80f, 0.88f, 1.0f}}},
    false,
    {0.42f, 0.68f, 0.52f, 0.85f},
    {0.50f, 0.58f, 0.88f, 0.85f},
    {0.62f, 0.85f, 0.70f, 1.0f},
    {0.70f, 0.76f, 0.98f, 1.0f},
    1.2f,
    1.2f,
    {0.90f, 0.92f, 0.95f, 0.90f},
    {0.92f, 0.94f, 0.97f, 1.0f},
    {},
    {0.24f, 0.25f, 0.27f, 1.0f},  // veri yok: notr koyu gri (arka plandan ve sudan farkli)
};

// VFR: sectional benzeri. Kuleli mavi, kulesiz macenta; Victor yollari kalin acik mavi.
const MapTheme kVfr = {
    MapMode::Vfr,
    "vfr",
    "VFR",
    {0.80f, 0.85f, 0.80f, 1.0f},
    {0.64f, 0.80f, 0.90f, 1.0f},
    TerrainStyle::Elevation,
    kWhiteHalo,
    true,
    {0.25f, 0.30f, 0.45f, 0.30f},
    {0.15f, 0.20f, 0.35f, 1.0f},
    {{{0.16f, 0.36f, 0.70f, 1.0f},
      {0.66f, 0.18f, 0.52f, 1.0f},
      {0.16f, 0.36f, 0.70f, 1.0f},
      {0.66f, 0.18f, 0.52f, 1.0f},
      {0.40f, 0.40f, 0.42f, 1.0f}}},
    {{{0.10f, 0.25f, 0.55f, 1.0f},
      {0.52f, 0.12f, 0.40f, 1.0f},
      {0.10f, 0.25f, 0.55f, 1.0f},
      {0.52f, 0.12f, 0.40f, 1.0f},
      {0.30f, 0.30f, 0.32f, 1.0f}}},
    true,
    {0.16f, 0.36f, 0.70f, 1.0f},
    {0.66f, 0.18f, 0.52f, 1.0f},
    false,
    {{{0.12f, 0.32f, 0.62f, 1.0f}, {0.55f, 0.25f, 0.50f, 1.0f}, {0.70f, 0.40f, 0.10f, 1.0f},
      {0.25f, 0.30f, 0.38f, 1.0f}}},
    true,
    {0.30f, 0.55f, 0.85f, 0.45f},
    {0.25f, 0.35f, 0.60f, 0.60f},
    {0.12f, 0.30f, 0.60f, 1.0f},
    {0.15f, 0.22f, 0.45f, 1.0f},
    3.0f,
    1.2f,
    {0.95f, 0.96f, 0.98f, 0.90f},
    {0.10f, 0.15f, 0.30f, 1.0f},
    {{{"terrain", true},
      {"ils", false},
      {"fix", false},
      {"airways_low", true},
      {"airways_high", false}}},
    {0.86f, 0.86f, 0.84f, 1.0f},  // veri yok: notr acik gri (su ve karadan farkli)
};

// IFR Low: enroute low benzeri. IAP'li sayilan buyukler yesil, digerleri kahverengi.
const MapTheme kIfrLow = {
    MapMode::IfrLow,
    "ifr_low",
    "IFR Low",
    {0.95f, 0.95f, 0.92f, 1.0f},
    {0.84f, 0.90f, 0.93f, 1.0f},
    TerrainStyle::LandWater,
    kWhiteHalo,
    true,
    {0.45f, 0.50f, 0.55f, 0.25f},
    {0.35f, 0.40f, 0.45f, 1.0f},
    {{{0.15f, 0.50f, 0.30f, 1.0f},
      {0.50f, 0.36f, 0.22f, 1.0f},
      {0.50f, 0.36f, 0.22f, 1.0f},
      {0.50f, 0.36f, 0.22f, 1.0f},
      {0.55f, 0.55f, 0.55f, 1.0f}}},
    {{{0.10f, 0.40f, 0.22f, 1.0f},
      {0.42f, 0.30f, 0.18f, 1.0f},
      {0.42f, 0.30f, 0.18f, 1.0f},
      {0.42f, 0.30f, 0.18f, 1.0f},
      {0.45f, 0.45f, 0.45f, 1.0f}}},
    false,
    {},
    {},
    false,
    {{{0.10f, 0.30f, 0.60f, 1.0f}, {0.50f, 0.30f, 0.55f, 1.0f}, {0.65f, 0.42f, 0.15f, 1.0f},
      {0.20f, 0.25f, 0.30f, 1.0f}}},
    true,
    {0.30f, 0.36f, 0.42f, 0.85f},
    {0.35f, 0.45f, 0.70f, 0.60f},
    {0.20f, 0.25f, 0.30f, 1.0f},
    {0.20f, 0.30f, 0.55f, 1.0f},
    1.4f,
    1.2f,
    {0.25f, 0.27f, 0.30f, 0.90f},
    {0.15f, 0.18f, 0.22f, 1.0f},
    {{{"terrain", true},
      {"ils", true},
      {"fix", true},
      {"airways_low", true},
      {"airways_high", false}}},
    {0.86f, 0.86f, 0.86f, 1.0f},  // veri yok: notr gri
};

// IFR High: yuksek hava yollari, sadece buyuk havalimanlari, NDB yok
const MapTheme kIfrHigh = {
    MapMode::IfrHigh,
    "ifr_high",
    "IFR High",
    {0.93f, 0.95f, 0.97f, 1.0f},
    {0.84f, 0.90f, 0.93f, 1.0f},
    TerrainStyle::LandWater,
    kWhiteHalo,
    true,
    {0.45f, 0.50f, 0.58f, 0.25f},
    {0.35f, 0.40f, 0.48f, 1.0f},
    kIfrLow.mAirport,
    kIfrLow.mAirportLabel,
    false,
    {},
    {},
    true,
    kIfrLow.mNavaid,
    true,
    {0.30f, 0.36f, 0.42f, 0.85f},
    {0.20f, 0.40f, 0.70f, 0.85f},
    {0.20f, 0.25f, 0.30f, 1.0f},
    {0.12f, 0.28f, 0.55f, 1.0f},
    1.2f,
    1.4f,
    {0.25f, 0.27f, 0.30f, 0.90f},
    {0.15f, 0.18f, 0.22f, 1.0f},
    {{{"terrain", true},
      {"ils", false},
      {"ndb", false},
      {"fix", true},
      {"airways_low", false},
      {"airways_high", true}}},
    {0.86f, 0.86f, 0.86f, 1.0f},  // veri yok: notr gri
};

}  // namespace

const MapTheme& ThemeFor(MapMode pMode)
{
    switch (pMode)
    {
        case MapMode::Vfr:
            return kVfr;
        case MapMode::IfrLow:
            return kIfrLow;
        case MapMode::IfrHigh:
            return kIfrHigh;
        default:
            return kScenery;
    }
}

TerrainImageStyle TerrainStyleFor(MapMode pMode)
{
    const MapTheme& tTheme = ThemeFor(pMode);
    auto tToByte = [](float pValue)
    {
        return static_cast<uint8_t>(pValue * 255.0f + 0.5f);
    };
    TerrainImageStyle tStyle;
    tStyle.mStyle = tTheme.mTerrainStyle;
    tStyle.mLand = {tToByte(tTheme.mBackground.mR), tToByte(tTheme.mBackground.mG), tToByte(tTheme.mBackground.mB)};
    tStyle.mWater = {tToByte(tTheme.mWater.mR), tToByte(tTheme.mWater.mG), tToByte(tTheme.mWater.mB)};
    return tStyle;
}

std::vector<TerrainImageStyle> AllTerrainStyles()
{
    std::vector<TerrainImageStyle> tStyles;
    for (int tIndex = 0; tIndex < kMapModeCount; ++tIndex)
    {
        tStyles.push_back(TerrainStyleFor(static_cast<MapMode>(tIndex)));
    }
    return tStyles;
}

MapMode ModeFromId(std::string_view pId, MapMode pFallback)
{
    for (int tIndex = 0; tIndex < kMapModeCount; ++tIndex)
    {
        const MapMode tMode = static_cast<MapMode>(tIndex);
        if (pId == ThemeFor(tMode).mId)
        {
            return tMode;
        }
    }
    return pFallback;
}

MapMode NextMode(MapMode pMode)
{
    return static_cast<MapMode>((static_cast<int>(pMode) + 1) % kMapModeCount);
}

std::string_view LayerSectionFor(MapMode pMode)
{
    switch (pMode)
    {
        case MapMode::Vfr:
            return "layers.vfr";
        case MapMode::IfrLow:
            return "layers.ifr_low";
        case MapMode::IfrHigh:
            return "layers.ifr_high";
        default:
            return "layers.scenery";
    }
}
