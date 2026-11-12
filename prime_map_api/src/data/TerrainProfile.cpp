// TerrainProfile.cpp
#include "data/TerrainProfile.h"

#include <array>
#include <format>

namespace TerrainProfile
{

namespace
{

const std::array<const char*, static_cast<size_t>(Stage::Count)> kNames = {
    "dsf_unpack", "dsf_triangles", "dsf_vectors", "overlays",   "fill_areas",    "cache_save",
    "elev_build", "elev_pack",     "load_raster", "load_elev",  "load_vectors",  "triangles",
    "img_classes", "img_blur",     "img_coast",   "img_compose", "img_detail",   "bleed",
};

struct Totals
{
    std::array<int, static_cast<size_t>(Stage::Count)> mCounts = {};
    std::array<double, static_cast<size_t>(Stage::Count)> mMilliseconds = {};
};

thread_local Totals gTotals;

}  // namespace

void Add(Stage pStage, double pMilliseconds)
{
    const size_t tIndex = static_cast<size_t>(pStage);
    ++gTotals.mCounts[tIndex];
    gTotals.mMilliseconds[tIndex] += pMilliseconds;
}

std::string TakeReport()
{
    std::string tText;
    for (size_t tIndex = 0; tIndex < kNames.size(); ++tIndex)
    {
        const int tCount = gTotals.mCounts[tIndex];
        if (tCount > 0)
        {
            tText += std::format(" {} {}x{:.0f}={:.0f}", kNames[tIndex], tCount, gTotals.mMilliseconds[tIndex] / tCount,
                                 gTotals.mMilliseconds[tIndex]);
        }
    }
    gTotals = Totals();
    return tText;
}

ScopedStage::ScopedStage(Stage pStage)
    : mStage(pStage)
    , mStart(std::chrono::steady_clock::now())
{
}

ScopedStage::~ScopedStage()
{
    Add(mStage, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - mStart).count());
}

}  // namespace TerrainProfile
