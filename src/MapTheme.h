// MapTheme.h
// Harita modlari ve her modun gorunumu (renkler, zemin bicimi, varsayilan
// katmanlar). X-Plane'e bagimli DEGIL.
//
//   Scenery  : arazi siniflari + kabartma (bugunku gorunum, X-Plane'de yok)
//   VFR      : rakima gore zemin renkleri, kuleli havalimani mavi / kulesiz
//              macenta, VOR pusula gulu, acik mavi Victor hava yollari
//   IFR Low  : zemin sadece kara / su (iki duz renk); alcak hava yollari, VOR/NDB/fix
//   IFR High : ayni zemin; yuksek hava yollari, sadece buyuk havalimanlari
//   Dark     : (0.18) tam siyah, minimal (ucus kontrol ekrani tarzi): kara siyah, su koyu lacivert,
//              havalimanlari, pistler, navaid'ler, ILS, hava yollari; arazi ayrintisi yok
//
// Katmanlar renkleri DrawContext::mTheme'den okur. Scenery temasi eski
// sabitlerin birebir aynisidir (gorunum degismesin).
//
// Renkler ilk surum: FAA harita geleneklerinden (sectional / enroute).
// X-Plane'in kendi haritasindan alinan ekran goruntuleriyle ayarlanacak.
#pragma once

#include <array>
#include <span>
#include <string_view>
#include <vector>

#include "Color.h"
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

    // 0.16: "veri yok" karosu (kara ama sahnesi bu bilgisayarda kurulu degil; dsf_files).
    // Deniz gibi boyanmasin diye notr, sudan ve arka plandan farkli bir renk.
    Rgba mNoData;

    // Asama 2: DSF yol agi (sira VectorLineKind: otoyol, ana, tali, yerel, demiryolu, enerji hatti)
    std::array<Rgba, 6> mRoads;
    // Yerlesim alanlari VFR / IFR'da bu renge karisir (sectional sarisi); Scenery'de sinif rengi kullanilir
    Rgba mBuiltUp;
};

const MapTheme& ThemeFor(MapMode pMode);

// Zemin iscisine verilecek goruntu bicimi (kara = arka plan, su = mWater)
TerrainImageStyle TerrainStyleFor(MapMode pMode);

// Butun modlarin bicimleri, MapMode sirasiyla (TerrainStore::Start'a verilir;
// TerrainImageKey::mStyle = static_cast<int>(MapMode))
std::vector<TerrainImageStyle> AllTerrainStyles();

// "vfr" -> MapMode::Vfr; taninmayan deger pFallback
MapMode ModeFromId(std::string_view pId, MapMode pFallback);

// Siradaki mod (dongusel): komut ozdev/map/next_mode
MapMode NextMode(MapMode pMode);

// Katman gorunurlugunun saklandigi ayar bolumu: "layers.vfr" ...
std::string_view LayerSectionFor(MapMode pMode);
