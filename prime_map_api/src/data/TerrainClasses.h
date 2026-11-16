// DSF arazi turu adini (TERT, ornek "lib/g10/terrain10/coni_tmp_wet_hill.ter")
// haritada cizilecek bir sinifa cevirir ve sinif renklerini tutar.
// X-Plane'e bagimli DEGIL.
#pragma once

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
    Land,      // taninmayan kara turu (ozel mesh'ler, ortofoto vb.)
};

const int kTerrainClassCount = 15;

TerrainClass classifyTerrain(const std::string& pTerrainName);

// Ayar dosyasindaki anahtar adi ("water", "forest_conifer" ...)
const char* terrainClassKey(TerrainClass pClass);

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
    bool mHasBuiltUp = false;            // Elevation / LandWater: yerlesim alanlari bu renge karisir (sectional sari)
    TerrainColor mBuiltUp = {0, 0, 0};
};

// VFR rakim rampasi (sectional benzeri: alcak yesil, orta sari/ten, yuksek kahverengi).
// [vfr] bands = 0 iken kullanilir (eski surekli rampa).
TerrainColor elevationColor(double pMeters);

// [relief]: kabartma golgesi. Her doku pikselinde, piksel boyuna uygun DEM piramit
// seviyesinden bikubik okunan egimle hesaplanir (TerrainElevation, TerrainRelief).
struct TerrainReliefParams
{
    bool mMultiDirectional = true;  // 4 yonlu isik (agirlik kuzeybatida); false: tek isik 315 derece
    double mExaggeration = 2.5;     // egim abartmasi (kucuk olcekte kabartma gorunsun)
    double mCoarseBoost = 0.35;     // kaba seviyelerde abartma artisi: (hucre / en ince hucre) ^ bu; 0 = sabit
    double mSlopeTone = 0.12;       // dik yamaclari isiktan bagimsiz koyulastirma (0 = kapali)
    double mFlatSlope = 0.02;       // bu egimin (~%2) altindaki egimler bastirilir: DEM'in 1 m basamaklari
                                    // duz ovada dalga gibi gorunmesin (0 = kapali)
};

// [water]: su ve kiyi
struct TerrainWaterParams
{
    bool mCoastline = true;           // kara/su sinirinda ince, kenari yumusak cizgi (IFR dahil)
    double mCoastlineStrength = 0.5;  // cizginin koyulugu 0..1
    double mBandMeters = 200.0;       // kiyidan aciga acik serit genisligi (0 = kapali)
    double mBandMaxPixels = 24.0;     // seridin doku pikseli tavani (yakin zoomda ekrani kaplamasin)
    double mBandStrength = 0.10;      // seridin aciklik orani 0..1
    double mDepthTone = 0.25;         // sea_level derinligine gore koyulasma 0..1 (0 = kapali)
    double mDepthFullMeters = 200.0;  // bu derinlikte koyulasma tam olur
};

// [vfr]: sectional rakim bantlari. Bant sinirlari keskindir; piksel basina bikubik
// rakimdan hesaplandigi icin her zoomda kivrimli kontur olarak gorunur, kenari
// bir piksel genisliginde yumusatilir.
const int kVfrBandCount = 8;
struct TerrainVfrParams
{
    bool mBands = true;              // false: eski surekli rampa (ElevationColor)
    bool mContourLines = false;      // bant sinirlarinda ince, soluk kontur cizgisi
    double mContourStrength = 0.35;  // cizginin koyulugu 0..1
    std::array<double, kVfrBandCount> mBandFeet;         // her bandin alt siniri (feet), artan
    std::array<TerrainColor, kVfrBandCount> mBandColors;  // varsayilan: ElevationColor duraklari
};

// Sectional siniri: 0 / 1000 / 2000 / 3000 / 5000 / 7000 / 9000 / 12000+ ft
TerrainVfrParams defaultVfrParams();

class TerrainPalette
{
public:
    // varsayilan renkler degisince artar (dosyadaki eski renkler atilir).
    // 3: renkler X-Plane dokularindan
    static const int kPaletteVersion = 3;

    TerrainPalette();

    // [colors] bolumu: <sinif> = R G B (0..255); [shading] strength = 0..1;
    // [palette] version: varsayilan renklerin surumu; [blend] meters/max_pixels:
    // Scenery'de arazi siniflari arasi yumusak gecis (metre; yakin zoomda piksel tavani);
    // [warp] meters/wavelength: kara siniflarinin dogal (kivrilmis) sinirlari;
    // [closeup] class_mix: yakin zoomda sinif renklerinin ortak kara tona karismasi
    void load(const Settings& pSettings);
    void save(Settings& pSettings) const;

    const TerrainColor& colorOf(TerrainClass pClass) const;
    double shadingStrength() const;  // kabartma golgesinin gucu (0 = yok)

    // [blend] meters: Scenery'de arazi siniflari arasi yumusak gecis mesafesi (0 = kapali)
    double blendMeters() const;
    void setBlendMeters(double pMeters);  // testler ve ileride ayar penceresi icin

    // [blend] max_pixels: yumusatma eriminin doku pikseli tavani (0 = tavan yok)
    double blendMaxPixels() const;
    void setBlendMaxPixels(double pPixels);

    // [warp] meters/wavelength: kara sinirlarini buken gurultu (meters = en buyuk kayma, 0 = duz kenarlar)
    const TerrainWarpParams& warp() const;
    void setWarp(const TerrainWarpParams& pParams);

    // [closeup] class_mix: yakin zoomda kara sinif renklerinin ortak kara tona karisma orani (0 = kapali)
    double closeupClassMix() const;
    void setCloseupClassMix(double pMix);

    // Bitki ortusu siniflarinin ortalamasi ([closeup] karisiminin referansi)
    TerrainColor referenceLandColor() const;

    // Yakin zoom sinif karisimi: piksel boyu 50 m ve ustu -> 0, 5 m ve alti -> tam oran; arasi logaritmik
    double closeupMixAt(double pMetersPerPixel) const;

    // Bu doku icin sinif renkleri: pMix > 0 iken kara siniflari (su, kar, None haric) referans
    // kara tona dogru pMix oraninda karisir
    std::array<TerrainColor, kTerrainClassCount> mixedColors(double pMix) const;

    // Load sonrasi: dosyadaki [palette] version guncel miydi (false: renkler varsayilandan, dosya yeniden yazilmali)
    bool loadedCurrentVersion() const;

    // [palette] source = xplane (varsayilan): sinif renkleri X-Plane'in arazi dokularindan
    // (TerrainPaletteSource); ini: [colors] bolumundeki renkler. Su rengi her zaman [colors] / varsayilan.
    bool usesXPlaneColors() const;
    // X-Plane dokularindan hesaplanan renkler (pCounts[i] > 0 olan siniflar; su haric) uygulanir
    void applyDerivedColors(const std::array<TerrainColor, kTerrainClassCount>& pColors,
                            const std::array<int, kTerrainClassCount>& pCounts);

    // [relief], [water], [vfr]: renk surumune bagli degil, her zaman okunur
    const TerrainReliefParams& relief() const;
    void setRelief(const TerrainReliefParams& pParams);
    const TerrainWaterParams& water() const;
    void setWater(const TerrainWaterParams& pParams);
    const TerrainVfrParams& vfr() const;
    void setVfr(const TerrainVfrParams& pParams);

private:
    TerrainColor mColors[kTerrainClassCount];
    double mShadingStrength;
    double mBlendMeters;
    double mBlendMaxPixels;
    TerrainWarpParams mWarp;
    double mCloseupClassMix;
    bool mLoadedCurrentVersion = true;
    bool mUseXPlaneColors = true;
    TerrainReliefParams mRelief;
    TerrainWaterParams mWater;
    TerrainVfrParams mVfr;
};
