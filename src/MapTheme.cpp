// MapTheme.cpp
#include "MapTheme.h"

namespace
{

// Ortak acik tema parcalari (VFR / IFR): beyaz kontur, koyu yazi
const Rgba kWhiteHalo = {1.0f, 1.0f, 1.0f, 0.85f};

// Varsayilan katman gorunurlugu, mod basina (ayar dosyasinda deger yoksa kullanilir).
// Scenery / VFR / IFR: kullanicinin 2026-11-07'de X-Plane'de kaydettigi ayarlar (0.18'de varsayilan yapildi).
// Dark: minimal harita + hava yollari (kullanici karari). Listede olmayan katman Layer'in kendi varsayilanini alir.
const LayerDefault kSceneryLayers[] = {
    {"aircraft", true}, {"airport_objects", false}, {"airport_objects.approach_lights", false},
    {"airport_objects.beacons", false}, {"airport_objects.gates", false}, {"airport_objects.hangars", false},
    {"airport_objects.helipads", false}, {"airport_objects.lighting", false}, {"airport_objects.other_starts", false},
    {"airport_objects.signs", false}, {"airport_objects.start_names", false}, {"airport_objects.tie_downs", false},
    {"airport_objects.windsocks", false}, {"airport_surfaces", true}, {"airport_surfaces.boundary", true},
    {"airport_surfaces.markings", false}, {"airport_surfaces.pavement", true},
    {"airport_surfaces.pavement_edges", true}, {"airports", true}, {"airways_high", true}, {"airways_low", true},
    {"drawings", true}, {"drawings.labels", true}, {"drawings.points", true}, {"drawings.regions", true},
    {"drawings.routes", true}, {"fix", false}, {"fms", true}, {"grid", true}, {"ils", false}, {"ndb", true},
    {"roads", false}, {"roads.local", false}, {"roads.motorways", false}, {"roads.power_lines", false},
    {"roads.primary", false}, {"roads.railways", false}, {"roads.secondary", false}, {"runways", true},
    {"runways.markings", true}, {"runways.numbers", true}, {"runways.overruns", true}, {"runways.paved", true},
    {"runways.unpaved", true}, {"runways.water", true}, {"taxi_routes", false}, {"terrain", true},
    {"terrain.built_up", true}, {"terrain.forests", false}, {"track", true}, {"vor", true},
};

const LayerDefault kVfrLayers[] = {
    {"aircraft", true}, {"airport_objects", true}, {"airport_objects.approach_lights", false},
    {"airport_objects.beacons", false}, {"airport_objects.gates", false}, {"airport_objects.hangars", false},
    {"airport_objects.helipads", true}, {"airport_objects.lighting", false}, {"airport_objects.other_starts", false},
    {"airport_objects.signs", false}, {"airport_objects.start_names", false}, {"airport_objects.tie_downs", false},
    {"airport_objects.windsocks", false}, {"airport_surfaces", true}, {"airport_surfaces.boundary", true},
    {"airport_surfaces.markings", false}, {"airport_surfaces.pavement", true},
    {"airport_surfaces.pavement_edges", false}, {"airports", true}, {"airways_high", false}, {"airways_low", true},
    {"drawings", true}, {"drawings.labels", true}, {"drawings.points", true}, {"drawings.regions", true},
    {"drawings.routes", true}, {"fix", false}, {"fms", true}, {"grid", true}, {"ils", false}, {"ndb", true},
    {"roads", false}, {"roads.local", true}, {"roads.motorways", true}, {"roads.power_lines", true},
    {"roads.primary", true}, {"roads.railways", true}, {"roads.secondary", true}, {"runways", true},
    {"runways.markings", true}, {"runways.numbers", true}, {"runways.overruns", true}, {"runways.paved", true},
    {"runways.unpaved", true}, {"runways.water", true}, {"taxi_routes", false}, {"terrain", true},
    {"terrain.built_up", true}, {"terrain.forests", true}, {"track", true}, {"vor", true},
};

const LayerDefault kIfrLowLayers[] = {
    {"aircraft", true}, {"airport_objects", false}, {"airport_objects.approach_lights", false},
    {"airport_objects.beacons", false}, {"airport_objects.gates", false}, {"airport_objects.hangars", false},
    {"airport_objects.helipads", false}, {"airport_objects.lighting", false}, {"airport_objects.other_starts", false},
    {"airport_objects.signs", false}, {"airport_objects.start_names", false}, {"airport_objects.tie_downs", false},
    {"airport_objects.windsocks", false}, {"airport_surfaces", true}, {"airport_surfaces.boundary", true},
    {"airport_surfaces.markings", false}, {"airport_surfaces.pavement", true},
    {"airport_surfaces.pavement_edges", true}, {"airports", true}, {"airways_high", false}, {"airways_low", true},
    {"drawings", true}, {"drawings.labels", true}, {"drawings.points", true}, {"drawings.regions", true},
    {"drawings.routes", true}, {"fix", false}, {"fms", true}, {"grid", true}, {"ils", false}, {"ndb", true},
    {"roads", false}, {"roads.local", true}, {"roads.motorways", true}, {"roads.power_lines", false},
    {"roads.primary", true}, {"roads.railways", true}, {"roads.secondary", true}, {"runways", true},
    {"runways.markings", true}, {"runways.numbers", true}, {"runways.overruns", true}, {"runways.paved", true},
    {"runways.unpaved", true}, {"runways.water", true}, {"taxi_routes", false}, {"terrain", true},
    {"terrain.built_up", false}, {"terrain.forests", true}, {"track", true}, {"vor", true},
};

const LayerDefault kIfrHighLayers[] = {
    {"aircraft", true}, {"airport_objects", true}, {"airport_objects.approach_lights", false},
    {"airport_objects.beacons", false}, {"airport_objects.gates", false}, {"airport_objects.hangars", false},
    {"airport_objects.helipads", true}, {"airport_objects.lighting", false}, {"airport_objects.other_starts", false},
    {"airport_objects.signs", false}, {"airport_objects.start_names", false}, {"airport_objects.tie_downs", false},
    {"airport_objects.windsocks", false}, {"airport_surfaces", true}, {"airport_surfaces.boundary", true},
    {"airport_surfaces.markings", false}, {"airport_surfaces.pavement", true},
    {"airport_surfaces.pavement_edges", true}, {"airports", true}, {"airways_high", true}, {"airways_low", false},
    {"drawings", true}, {"drawings.labels", true}, {"drawings.points", true}, {"drawings.regions", true},
    {"drawings.routes", true}, {"fix", true}, {"fms", true}, {"grid", true}, {"ils", false}, {"ndb", true},
    {"roads", false}, {"roads.local", true}, {"roads.motorways", true}, {"roads.power_lines", false},
    {"roads.primary", true}, {"roads.railways", true}, {"roads.secondary", true}, {"runways", true},
    {"runways.markings", true}, {"runways.numbers", true}, {"runways.overruns", true}, {"runways.paved", true},
    {"runways.unpaved", true}, {"runways.water", true}, {"taxi_routes", false}, {"terrain", true},
    {"terrain.built_up", false}, {"terrain.forests", true}, {"track", true}, {"vor", true},
};

const LayerDefault kDarkLayers[] = {
    {"aircraft", true}, {"airport_objects", false}, {"airport_objects.approach_lights", false},
    {"airport_objects.beacons", false}, {"airport_objects.gates", false}, {"airport_objects.hangars", false},
    {"airport_objects.helipads", false}, {"airport_objects.lighting", false}, {"airport_objects.other_starts", false},
    {"airport_objects.signs", false}, {"airport_objects.start_names", false}, {"airport_objects.tie_downs", false},
    {"airport_objects.windsocks", false}, {"airport_surfaces", true}, {"airport_surfaces.boundary", false},
    {"airport_surfaces.markings", false}, {"airport_surfaces.pavement", true},
    {"airport_surfaces.pavement_edges", true}, {"airports", true}, {"airways_high", true}, {"airways_low", true},
    {"drawings", true}, {"drawings.labels", true}, {"drawings.points", true}, {"drawings.regions", true},
    {"drawings.routes", true}, {"fix", true}, {"fms", true}, {"grid", false}, {"ils", true}, {"ndb", true},
    {"roads", false}, {"roads.local", false}, {"roads.motorways", false}, {"roads.power_lines", false},
    {"roads.primary", false}, {"roads.railways", false}, {"roads.secondary", false}, {"runways", true},
    {"runways.markings", true}, {"runways.numbers", true}, {"runways.overruns", true}, {"runways.paved", true},
    {"runways.unpaved", true}, {"runways.water", true}, {"taxi_routes", false}, {"terrain", true},
    {"terrain.built_up", false}, {"terrain.forests", false}, {"track", true}, {"vor", true},
};


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
    kSceneryLayers,
    {0.24f, 0.25f, 0.27f, 1.0f},  // veri yok: notr koyu gri (arka plandan ve sudan farkli)
    {{{0.95f, 0.72f, 0.38f, 0.90f},  // otoyol
      {0.92f, 0.88f, 0.74f, 0.85f},  // ana yol
      {0.86f, 0.86f, 0.80f, 0.70f},  // tali
      {0.80f, 0.80f, 0.78f, 0.45f},  // yerel
      {0.62f, 0.56f, 0.66f, 0.90f},  // demiryolu
      {0.70f, 0.72f, 0.78f, 0.55f}}},  // enerji hatti
    {0.0f, 0.0f, 0.0f, 0.0f},  // yerlesim: Scenery'de sinif rengi
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
    kVfrLayers,
    {0.86f, 0.86f, 0.84f, 1.0f},  // veri yok: notr acik gri (su ve karadan farkli)
    {{{0.20f, 0.20f, 0.24f, 0.90f},  // sectional: yollar koyu gri, demiryolu siyah
      {0.28f, 0.28f, 0.32f, 0.85f},
      {0.36f, 0.36f, 0.40f, 0.70f},
      {0.45f, 0.45f, 0.48f, 0.50f},
      {0.10f, 0.10f, 0.12f, 0.90f},
      {0.45f, 0.35f, 0.55f, 0.70f}}},
    {0.98f, 0.86f, 0.42f, 1.0f},  // yerlesim: sectional sarisi
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
    kIfrLowLayers,
    {0.86f, 0.86f, 0.86f, 1.0f},  // veri yok: notr gri
    {{{0.55f, 0.55f, 0.58f, 0.80f},  // IFR: soluk gri (varsayilan kapali)
      {0.62f, 0.62f, 0.65f, 0.70f},
      {0.68f, 0.68f, 0.70f, 0.60f},
      {0.74f, 0.74f, 0.76f, 0.45f},
      {0.45f, 0.45f, 0.48f, 0.80f},
      {0.60f, 0.55f, 0.70f, 0.60f}}},
    {0.93f, 0.88f, 0.74f, 1.0f},  // yerlesim: soluk bej
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
    kIfrHighLayers,
    {0.86f, 0.86f, 0.86f, 1.0f},  // veri yok: notr gri
    {{{0.55f, 0.55f, 0.58f, 0.80f},  // IFR: soluk gri (varsayilan kapali)
      {0.62f, 0.62f, 0.65f, 0.70f},
      {0.68f, 0.68f, 0.70f, 0.60f},
      {0.74f, 0.74f, 0.76f, 0.45f},
      {0.45f, 0.45f, 0.48f, 0.80f},
      {0.60f, 0.55f, 0.70f, 0.60f}}},
    {0.93f, 0.88f, 0.74f, 1.0f},  // yerlesim: soluk bej
};

// Dark (0.18): ucus kontrol / radar ekrani tarzi tam siyah minimal harita. Zemin sadece
// kara (siyah) ve su (koyu lacivert); kabartma, sinif, yol, orman yok. Parlak ama
// doygun olmayan renkler: karanlik IOS odasinda goz yormasin.
const MapTheme kDark = {
    MapMode::Dark,
    "dark",
    "Dark",
    {0.0f, 0.0f, 0.0f, 1.0f},     // kara: siyah
    {0.03f, 0.07f, 0.14f, 1.0f},  // su: koyu lacivert (kara ile ton farki yeterli)
    TerrainStyle::LandWater,
    {0.0f, 0.0f, 0.0f, 0.85f},  // siyah kontur
    false,
    {0.55f, 0.60f, 0.70f, 0.14f},
    {0.50f, 0.55f, 0.62f, 1.0f},
    {{{0.35f, 0.85f, 1.00f, 1.0f},  // buyuk
      {0.55f, 0.72f, 0.82f, 1.0f},  // kucuk
      {0.35f, 0.62f, 0.95f, 1.0f},  // deniz
      {0.72f, 0.62f, 0.92f, 1.0f},  // heliport
      {0.45f, 0.46f, 0.50f, 1.0f}}},  // kapali
    {{{0.80f, 0.94f, 1.00f, 1.0f},
      {0.66f, 0.78f, 0.86f, 1.0f},
      {0.60f, 0.75f, 0.95f, 1.0f},
      {0.78f, 0.72f, 0.92f, 1.0f},
      {0.55f, 0.56f, 0.60f, 1.0f}}},
    false,
    {},
    {},
    false,
    {{{0.35f, 0.90f, 0.55f, 1.0f},   // VOR: yesil
      {0.95f, 0.65f, 0.30f, 1.0f},   // NDB: turuncu
      {0.95f, 0.85f, 0.35f, 1.0f},   // ILS: sari
      {0.70f, 0.74f, 0.80f, 1.0f}}},  // fix: gri
    false,
    {0.40f, 0.52f, 0.62f, 0.55f},
    {0.52f, 0.48f, 0.72f, 0.55f},
    {0.55f, 0.68f, 0.78f, 1.0f},
    {0.66f, 0.62f, 0.86f, 1.0f},
    1.0f,
    1.0f,
    {0.92f, 0.94f, 0.97f, 0.95f},
    {0.94f, 0.96f, 0.98f, 1.0f},
    kDarkLayers,
    {0.08f, 0.08f, 0.09f, 1.0f},  // veri yok: siyaha yakin gri
    {{{0.40f, 0.40f, 0.44f, 0.80f},  // yollar (varsayilan kapali): koyu gri
      {0.34f, 0.34f, 0.38f, 0.70f},
      {0.30f, 0.30f, 0.33f, 0.60f},
      {0.26f, 0.26f, 0.28f, 0.45f},
      {0.36f, 0.32f, 0.42f, 0.80f},
      {0.36f, 0.34f, 0.44f, 0.55f}}},
    {0.0f, 0.0f, 0.0f, 0.0f},  // yerlesim: yok
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
        case MapMode::Dark:
            return kDark;
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
    tStyle.mHasBuiltUp = tTheme.mBuiltUp.mA > 0.0f;
    tStyle.mBuiltUp = {tToByte(tTheme.mBuiltUp.mR), tToByte(tTheme.mBuiltUp.mG), tToByte(tTheme.mBuiltUp.mB)};
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
        case MapMode::Dark:
            return "layers.dark";
        default:
            return "layers.scenery";
    }
}
