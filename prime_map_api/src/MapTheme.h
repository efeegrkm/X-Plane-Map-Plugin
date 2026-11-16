// Harita modlari ve her modun gorunumu (renkler, zemin bicimi, varsayilan
// katmanlar). X-Plane'e bagimli DEGIL.
#pragma once

#include <array>
#include <span>
#include <string_view>
#include <vector>

#include "Color.h"
#include "PrimeMap.h"
#include "data/TerrainClasses.h"

enum class MapMode
{
    Scenery,
    Vfr,
    IfrLow,
    IfrHigh,
    Dark,
};

const int kMapModeCount = 5;

// Bir katmanin bu moddaki varsayilan gorunurlugu (ayar dosyasinda yoksa)
struct LayerDefault
{
    const char* mId;
    bool mVisible;
};

struct MapTheme
{
    MapMode mMode;
    const char* mId;    // ayar degeri: "scenery", "vfr", "ifr_low", "ifr_high", "dark"
    const char* mName;  // arayuz: "Scenery", "VFR" ...

    Rgba mBackground;  // zemin verisi olmayan / zeminsiz modda arka plan
    Rgba mWater;       // VFR / IFR'de su ve deniz karolari (Scenery'de palet kullanilir)
    TerrainStyle mTerrainStyle;
    Rgba mHalo;        // sembol konturu ve yazi golgesi
    bool mLight;       // acik arka plan: yazilar koyu, konturlar acik

    Rgba mGridLine;
    Rgba mGridLabel;

    // Havalimani sembolleri: sira AirportLayer::Category (buyuk, kucuk, deniz, heliport, kapali)
    std::array<Rgba, 5> mAirport;
    std::array<Rgba, 5> mAirportLabel;
    bool mAirportByTower;     // VFR: kuleli mavi, kulesiz macenta (deniz/heliport/kapali haric)
    Rgba mAirportTowered;
    Rgba mAirportUntowered;
    bool mMajorAirportsOnly;  // IFR High: sadece buyuk havalimanlari

    // Navaid renkleri: sira NavaidType (VOR, NDB, ILS, fix)
    std::array<Rgba, 4> mNavaid;
    bool mVorRose;  // VOR cevresinde 10 derece centikli pusula gulu

    Rgba mAirwayLow;
    Rgba mAirwayHigh;
    Rgba mAirwayLowLabel;
    Rgba mAirwayHighLabel;
    float mAirwayLowWidth;
    float mAirwayHighWidth;

    Rgba mRunwayEdge;
    Rgba mRunwayLabel;

    // Katman ve alt katman ("<katman>.<alt>") varsayilanlari; listede olmayan: Layer'in kendi varsayilani
    std::span<const LayerDefault> mLayerDefaults;

    // "veri yok" karosu (kara ama sahnesi bu bilgisayarda kurulu degil):
    // sudan ve arka plandan farkli notr renk.
    Rgba mNoData;

    // DSF yol agi (sira VectorLineKind: otoyol, ana, tali, yerel, demiryolu, enerji hatti)
    std::array<Rgba, 6> mRoads;
    // Yerlesim alanlari VFR / IFR'da bu renge karisir (sectional sarisi); Scenery'de sinif rengi kullanilir
    Rgba mBuiltUp;
};

const MapTheme& themeFor(MapMode pMode);

// API setColor: katmanin tema rengini temsilen yazma yuvasi (yoksa nullptr).
// Sadece dogrudan tema rengi olan katmanlar eslenir; nullptr donen katmanin rengi degistirilemez.
Rgba* themeLayerColor(MapTheme& pTheme, prime::MapLayer pLayer);

// Zemin iscisine verilecek goruntu bicimi (kara = arka plan, su = mWater)
TerrainImageStyle terrainStyleFor(MapMode pMode);

// TerrainImageKey::mStyle = static_cast<int>(MapMode) eslesir
std::vector<TerrainImageStyle> allTerrainStyles();

// "vfr" -> MapMode::Vfr; taninmayan deger pFallback
MapMode modeFromId(std::string_view pId, MapMode pFallback);

// Siradaki mod (dongusel): komut prime/map/next_mode
MapMode nextMode(MapMode pMode);

// Katman gorunurlugunun saklandigi ayar bolumu: "layers.vfr" ...
std::string_view layerSectionFor(MapMode pMode);
