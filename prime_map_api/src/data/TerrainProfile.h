// TerrainProfile.h
// Zemin iscisinin asama sureleri (olcum icin). X-Plane'e bagimli DEGIL.
//
// Her asama "ScopedStage" ile olculur; sureler thread'e ozel (thread_local) toplanir:
// zemin iscisi, onizleme ve testler birbirini bozmaz, kilit yok. Isci bosta kalinca
// TakeReport ile ozet satiri alinir ve sayaclar sifirlanir.
//
//   tek karo uretimi : DsfUnpack -> DsfTriangles -> DsfVectors -> Overlays -> FillAreas -> CacheSave
//                      -> ElevationBuild -> ElevationPack
//   onbellekten      : LoadRaster, LoadElevation, LoadVectors
//   yakin zoom       : Triangles (DSF ucgenleri)
//   doku             : ImageClasses (sinif ornekleme + bukme), ImageBlur, ImageCoast (kiyi cizgisi + uzaklik),
//                      ImageCompose (renk, kabartma, VFR, su), ImageDetail (ucgenlerden boyama), Bleed
#pragma once

#include <chrono>
#include <string>

namespace TerrainProfile
{

enum class Stage
{
    DsfUnpack,
    DsfTriangles,
    DsfVectors,
    Overlays,
    FillAreas,
    CacheSave,
    ElevationBuild,
    ElevationPack,
    LoadRaster,
    LoadElevation,
    LoadVectors,
    Triangles,
    ImageClasses,
    ImageBlur,
    ImageCoast,
    ImageCompose,
    ImageDetail,
    Bleed,
    Count,
};

void Add(Stage pStage, double pMilliseconds);

// "asama adet x ortalama ms (toplam)" satiri; bossa "". Sayaclari sifirlar.
std::string TakeReport();

class ScopedStage
{
public:
    explicit ScopedStage(Stage pStage);
    ~ScopedStage();

    ScopedStage(const ScopedStage&) = delete;
    ScopedStage& operator=(const ScopedStage&) = delete;

private:
    Stage mStage;
    std::chrono::steady_clock::time_point mStart;
};

}  // namespace TerrainProfile
