// DSF arazi turu adini ("lib/g10/terrain10/coni_tmp_wet_hill.ter") cizim sinifina cevirir
// ve sinif renklerini tutar. X-Plane'e bagimli DEGIL.
#ifndef PRIME_MAP_SRC_DATA_TERRAINCLASSES_H
#define PRIME_MAP_SRC_DATA_TERRAINCLASSES_H

#include <array>
#include <cstdint>
#include <string>

#include "data/TerrainNoise.h"

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
    Land,  // taninmayan kara turu (ozel mesh, ortofoto ...)
};

const int kTerrainClassCount = 15;

TerrainClass classifyTerrain(const std::string& pTerrainName);

const char* terrainClassKey(TerrainClass pClass);  // ayar dosyasi anahtari ("water", "forest_conifer" ...)

struct TerrainColor
{
    uint8_t mR;
    uint8_t mG;
    uint8_t mB;
};

enum class TerrainStyle
{
    Classes,    // arazi sinifi renkleri + kabartma
    Elevation,  // VFR: rakim rampasi + kabartma
    LandWater,  // IFR: kara ve su (iki duz renk), kabartmasiz
};

struct TerrainImageStyle
{
    TerrainStyle mStyle = TerrainStyle::Classes;
    TerrainColor mLand = {0, 0, 0};   // LandWater kara rengi
    TerrainColor mWater = {0, 0, 0};  // Elevation / LandWater su rengi
    bool mHasBuiltUp = false;         // yerlesim alanlari bu renge karisir (sectional sari)
    TerrainColor mBuiltUp = {0, 0, 0};
};

// VFR rakim rampasi; [vfr] bands = 0 iken kullanilir
TerrainColor elevationColor(double pMeters);

struct TerrainReliefParams
{
    bool mMultiDirectional = true;  // 4 yonlu isik (agirlik kuzeybatida); false: tek isik 315 derece
    double mExaggeration = 2.5;     // egim abartmasi
    double mCoarseBoost = 0.35;     // kaba seviyelerde abartma artisi; 0 = sabit
    double mSlopeTone = 0.12;       // dik yamaclari koyulastirma (0 = kapali)
    double mFlatSlope = 0.02;       // bu egim alti bastirilir: DEM'in 1 m basamaklari dalga gibi gorunmesin
};

struct TerrainWaterParams
{
    bool mCoastline = true;           // kara/su sinirinda ince cizgi (IFR dahil)
    double mCoastlineStrength = 0.5;  // 0..1
    double mBandMeters = 200.0;       // kiyidan aciga acik serit genisligi (0 = kapali)
    double mBandMaxPixels = 24.0;     // seridin doku pikseli tavani
    double mBandStrength = 0.10;      // 0..1
    double mDepthTone = 0.25;         // derinlige gore koyulasma 0..1 (0 = kapali)
    double mDepthFullMeters = 200.0;  // koyulasma tam olan derinlik
};

// Bant sinirlari piksel basina bikubik rakimdan hesaplanir: her zoomda kivrimli kontur gorunur.
const int kVfrBandCount = 8;
struct TerrainVfrParams
{
    bool mBands = true;              // false: surekli rampa (ElevationColor)
    bool mContourLines = false;      // bant sinirlarinda soluk kontur cizgisi
    double mContourStrength = 0.35;
    std::array<double, kVfrBandCount> mBandFeet;         // bandin alt siniri (feet), artan
    std::array<TerrainColor, kVfrBandCount> mBandColors;
};

// Sectional siniri: 0 / 1000 / 2000 / 3000 / 5000 / 7000 / 9000 / 12000+ ft
TerrainVfrParams defaultVfrParams();

class TerrainPalette
{
public:
    // varsayilan renkler degisince artar (dosyadaki eski renkler atilir)
    static const int kPaletteVersion = 3;

    TerrainPalette();

    // [colors] = <sinif> R G B; [shading], [blend], [warp], [closeup], [vfr] ve
    // [palette] version bolumlerini okur / yazar.
    void load(const Settings& pSettings);
    void save(Settings& pSettings) const;

    const TerrainColor& colorOf(TerrainClass pClass) const;
    double shadingStrength() const;  // kabartma golgesinin gucu (0 = yok)

    double blendMeters() const;           // siniflar arasi yumusak gecis (0 = kapali)
    void setBlendMeters(double pMeters);
    double blendMaxPixels() const;        // yumusatmanin doku pikseli tavani
    void setBlendMaxPixels(double pPixels);

    // [warp]: kara sinirlarini buken gurultu (meters = en buyuk kayma, 0 = duz kenarlar)
    const TerrainWarpParams& warp() const;
    void setWarp(const TerrainWarpParams& pParams);

    double closeupClassMix() const;  // yakin zoomda sinif renklerinin kara tona karisma orani
    void setCloseupClassMix(double pMix);

    // Bitki ortusu siniflarinin ortalamasi ([closeup] karisiminin referansi)
    TerrainColor referenceLandColor() const;

    // piksel boyu 50 m ustu -> 0, 5 m alti -> tam oran; arasi logaritmik
    double closeupMixAt(double pMetersPerPixel) const;

    // pMix > 0 iken kara siniflari (su, kar, None haric) referans tona dogru karisir
    std::array<TerrainColor, kTerrainClassCount> mixedColors(double pMix) const;

    // dosyadaki [palette] version guncel miydi (false: renkler varsayilandan)
    bool loadedCurrentVersion() const;

    // true (varsayilan): renkler X-Plane arazi dokularindan turetilir; false: [colors]
    bool usesXPlaneColors() const;
    void applyDerivedColors(const std::array<TerrainColor, kTerrainClassCount>& pColors,
                            const std::array<int, kTerrainClassCount>& pCounts);

    // renk surumune bagli degil, her zaman okunur
    const TerrainReliefParams& relief() const;
    void setRelief(const TerrainReliefParams& pParams);
    const TerrainWaterParams& water() const;
    void setWater(const TerrainWaterParams& pParams);
    const TerrainVfrParams& vfr() const;
    void setVfr(const TerrainVfrParams& pParams);

    // H-01: yerlesim karisim rengi ezmesi (setColor(TerrainBuiltUp)). Ezme yoksa goruntude
    // temanin mBuiltUp rengi kullanilir; ezmeli palet farkli anahtarla tani tir (registerPalette),
    // boylece ilgili dokular yeniden uretilir. [built_up] color = R G B; ezme yokken bolum yazilmaz.
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
    bool mBuiltUpOverride = false;   // setColor(TerrainBuiltUp) ezmesi aktif mi
    TerrainColor mBuiltUp{0, 0, 0};  // ezme rengi
    TerrainReliefParams mRelief;
    TerrainWaterParams mWater;
    TerrainVfrParams mVfr;
};
#endif  // PRIME_MAP_SRC_DATA_TERRAINCLASSES_H
