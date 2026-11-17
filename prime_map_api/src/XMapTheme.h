// Harita modlari ve her modun gorunumu (renkler, zemin bicimi, varsayilan
// katmanlar). X-Plane'e bagimli DEGIL.
// Harita modlari: tek tanim prime::XMapMode'dur (api/XMapTypes.h); motor ayni tipi kullanir.
#ifndef PRIME_MAP_SRC_XMAPTHEME_H
#define PRIME_MAP_SRC_XMAPTHEME_H

#include <array>
#include <span>
#include <string_view>
#include <vector>

#include "Color.h"
#include "api/XMapTypes.h"
#include "data/TerrainClasses.h"

using prime::XMapMode;  // motor kodu niteliksiz XMapMode yazar; ayni tip

const int kXMapModeCount = 5;

// Bir katmanin bu moddaki varsayilan gorunurlugu (ayar dosyasinda yoksa)
struct LayerDefault
{
    const char* mId;
    bool mVisible;
};

struct XMapTheme
{
    XMapMode mMode;
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

const XMapTheme& themeFor(XMapMode pMode);

// API setColor: katmanin tema rengini temsilen yazma yuvasi (yoksa nullptr).
// Sadece dogrudan tema rengi olan katmanlar eslenir; nullptr donen katmanin rengi degistirilemez.
Rgba* themeLayerColor(XMapTheme& pTheme, prime::XMapLayer pLayer);

// Zemin iscisine verilecek goruntu bicimi (kara = arka plan, su = mWater)
TerrainImageStyle terrainStyleFor(XMapMode pMode);

// TerrainImageKey::mStyle = static_cast<int>(XMapMode) eslesir
std::vector<TerrainImageStyle> allTerrainStyles();

// "vfr" -> XMapMode::Vfr; taninmayan deger pFallback
XMapMode modeFromId(std::string_view pId, XMapMode pFallback);

// Siradaki mod (dongusel): komut prime/map/next_mode
XMapMode nextMode(XMapMode pMode);

// Katman gorunurlugunun saklandigi ayar bolumu: "layers.vfr" ...
std::string_view layerSectionFor(XMapMode pMode);
#endif  // PRIME_MAP_SRC_XMAPTHEME_H
