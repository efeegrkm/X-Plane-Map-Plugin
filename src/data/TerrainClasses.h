// TerrainClasses.h
// DSF arazi turu adini (TERT, ornek "lib/g10/terrain10/coni_tmp_wet_hill.ter")
// haritada cizilecek bir sinifa cevirir ve sinif renklerini tutar.
// X-Plane'e bagimli DEGIL.
//
// Kurallar dosya adinin on ekine bakar (XP11 terrain10 kutuphanesindeki adlar;
// "sq_" on eki atilir). Gercek XP11 verisinde gorulen on ekler:
//   terrain_Water -> Water        urban -> Urban          coni -> ForestConifer
//   broad, rainforest -> ForestBroadleaf                   shrb -> Shrub
//   grass, pastr, sp, spr -> Grass                         crp, crpirg, orch, *crptwn* -> Crop
//   wetl, rivbd -> Wetland        rock, bare, mine, morn -> Rock
//   snow, ice -> SnowIce          tun -> Tundra           sand, dry, semidry -> Sand
//   apt -> Airport                digerleri -> Land
//
// Renkler kullanicinin degistirebilecegi bir dosyadadir (terrain_colors.ini).
#pragma once

#include <cstdint>
#include <string>

class Settings;

enum class TerrainClass : uint8_t
{
    None = 0,  // ucgen yok (cizilmez)
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
    Land,      // taninmayan kara turu (ozel mesh'ler, ortofoto vb.)
};

const int kTerrainClassCount = 15;

TerrainClass ClassifyTerrain(const std::string& pTerrainName);

// Ayar dosyasindaki anahtar adi ("water", "forest_conifer" ...)
const char* TerrainClassKey(TerrainClass pClass);

struct TerrainColor
{
    uint8_t mR;
    uint8_t mG;
    uint8_t mB;
};

// Zemin goruntusunun bicimi (harita moduna gore, MapTheme)
enum class TerrainStyle
{
    Classes,    // arazi sinifi renkleri (su, orman, sehir ...) + kabartma
    Elevation,  // VFR: rakim rampasi + kabartma, su ayri renk
    LandWater,  // IFR: sadece kara ve su (iki duz renk), kabartmasiz; kiyilar gorunur
};

struct TerrainImageStyle
{
    TerrainStyle mStyle = TerrainStyle::Classes;
    TerrainColor mLand = {0, 0, 0};   // LandWater: kara rengi
    TerrainColor mWater = {0, 0, 0};  // Elevation / LandWater: su rengi
};

// VFR rakim rampasi (sectional benzeri: alcak yesil, orta sari/ten, yuksek kahverengi)
TerrainColor ElevationColor(double pMeters);

class TerrainPalette
{
public:
    TerrainPalette();  // varsayilan renkler

    // [colors] bolumu: <sinif> = R G B (0..255); [shading] strength = 0..1
    void Load(const Settings& pSettings);
    void Save(Settings& pSettings) const;

    const TerrainColor& ColorOf(TerrainClass pClass) const;
    double ShadingStrength() const;  // kabartma golgesinin gucu (0 = yok)

private:
    TerrainColor mColors[kTerrainClassCount];
    double mShadingStrength;
};
