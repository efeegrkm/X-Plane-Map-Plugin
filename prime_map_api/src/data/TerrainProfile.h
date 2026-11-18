// Zemin uretim asamalarinin sure olcumu.
#ifndef PRIME_MAP_SRC_DATA_TERRAINPROFILE_H
#define PRIME_MAP_SRC_DATA_TERRAINPROFILE_H

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
}
#endif
