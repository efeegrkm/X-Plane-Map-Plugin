// Zemin iscisinin asama sureleri (olcum icin). X-Plane'e bagimli DEGIL.
// Sureler thread'e ozel (thread_local) toplanir: zemin iscisi, onizleme ve
// testler birbirini bozmaz, kilit yok. takeReport ozet alir, sayaclar sifirlanir.
//
//   tek karo uretimi : DsfUnpack -> DsfTriangles -> DsfVectors -> Overlays
//                      -> FillAreas -> CacheSave -> ElevationBuild -> ElevationPack
//   onbellekten      : loadRaster, loadElevation, loadVectors
//   yakin zoom       : triangles(DSF ucgenleri)
//   doku             : imageClasses, ImageBlur, imageCoast, imageCompose,
//                      imageDetail, Bleed
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
    loadRaster,
    loadElevation,
    loadVectors,
    Triangles,
    ImageClasses,
    ImageBlur,
    ImageCoast,
    ImageCompose,
    ImageDetail,
    Bleed,
    Count,
};

void add(Stage pStage, double pMilliseconds);

// "asama adet x ortalama ms (toplam)" satiri; bossa "". Sayaclari sifirlar.
std::string takeReport();

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
