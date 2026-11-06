// DsfInfoMain.cpp
// dsf_info: bir DSF dosyasinin ozetini yazar (gelistirme ve teshis icin).
//   dsf_info "<.../+47-123.dsf>"
// Ozellikler, arazi turu adlari, her arazi turunun ucgen sayisi (overlay ayri),
// koordinat araligi; overlay envanteri (nesne / poligon / ag tanimlari ve
// sayilari, dislama alanlari) ve asama sureleri (7z acma, okuma, ucgen gecisi,
// DEM). 7z ile sikistirilmis dosyalari kendisi acar.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "data/DsfReader.h"
#include "data/SevenZip.h"

namespace
{

// Bir onceki cagridan bu yana gecen sure (ms)
double ElapsedMs(std::chrono::steady_clock::time_point* pSince)
{
    const std::chrono::steady_clock::time_point tNow = std::chrono::steady_clock::now();
    const double tMs = std::chrono::duration<double, std::milli>(tNow - *pSince).count();
    *pSince = tNow;
    return tMs;
}

}  // namespace

int main(int pArgc, char** pArgv)
{
    if (pArgc < 2)
    {
        std::fprintf(stderr, "usage: dsf_info <file.dsf> [--raw out.dsf] [--dump-terrain <index> out.txt]\n");
        return 2;
    }
    // Secenekler: --raw <dosya>  acilmis DSF'i yazar
    //             --dump-terrain <indeks> <dosya>  o arazi turunun temel (overlay olmayan)
    //             ucgenlerini satir basina "lon lat lon lat lon lat" olarak yazar (olcum betikleri icin)
    std::string tRawPath;
    std::string tDumpPath;
    int tDumpTerrain = -1;
    for (int tArgument = 2; tArgument < pArgc; ++tArgument)
    {
        const std::string tName = pArgv[tArgument];
        if (tName == "--raw" && tArgument + 1 < pArgc)
        {
            tRawPath = pArgv[++tArgument];
        }
        else if (tName == "--dump-terrain" && tArgument + 2 < pArgc)
        {
            tDumpTerrain = std::atoi(pArgv[++tArgument]);
            tDumpPath = pArgv[++tArgument];
        }
        else
        {
            std::fprintf(stderr, "unknown option: %s\n", tName.c_str());
            return 2;
        }
    }
    std::ifstream tFile(pArgv[1], std::ios::binary);
    std::vector<uint8_t> tData((std::istreambuf_iterator<char>(tFile)), std::istreambuf_iterator<char>());
    std::printf("file: %s (%d bytes)\n", pArgv[1], static_cast<int>(tData.size()));
    std::chrono::steady_clock::time_point tClock = std::chrono::steady_clock::now();

    std::string tError;
    if (SevenZip::IsArchive(tData.data(), tData.size()))
    {
        std::vector<uint8_t> tPlain;
        if (!SevenZip::ExtractFirstFile(tData, &tPlain, &tError))
        {
            std::printf("7z error: %s\n", tError.c_str());
            return 1;
        }
        std::printf("7z archive -> %d bytes (%.0f ms)\n", static_cast<int>(tPlain.size()), ElapsedMs(&tClock));
        tData.swap(tPlain);
    }
    if (!tRawPath.empty())
    {
        // Acilmis DSF'i diske yaz (bicimi baska araclarla incelemek icin)
        std::ofstream tRaw(tRawPath, std::ios::binary);
        tRaw.write(reinterpret_cast<const char*>(tData.data()), static_cast<std::streamsize>(tData.size()));
        std::printf("raw DSF written: %s\n", tRawPath.c_str());
    }

    ElapsedMs(&tClock);  // --raw yazma suresi olcume girmesin
    DsfReader tReader;
    if (!tReader.Open(tData, &tError))
    {
        std::printf("DSF error: %s\n", tError.c_str());
        return 1;
    }
    const double tOpenMs = ElapsedMs(&tClock);
    const DsfInfo& tInfo = tReader.Info();
    std::printf("bounds: W %.3f S %.3f E %.3f N %.3f overlay=%d pools=%d\n", tInfo.mWest, tInfo.mSouth, tInfo.mEast,
                tInfo.mNorth, tInfo.mOverlay ? 1 : 0, tReader.PoolCount());
    for (const auto& tProperty : tInfo.mProperties)
    {
        if (tProperty.first.rfind("sim/", 0) == 0 && tProperty.first.rfind("sim/exclude_", 0) != 0)
        {
            std::printf("  prop %s = %s\n", tProperty.first.c_str(), tProperty.second.c_str());
        }
    }
    // Dislama alanlari (sim/exclude_obj, _fac, _for, _net, _pol, _str, _bch ...): tur basina sayi
    std::map<std::string, int> tExclusions;
    for (const auto& tProperty : tInfo.mProperties)
    {
        if (tProperty.first.rfind("sim/exclude_", 0) == 0)
        {
            tExclusions[tProperty.first]++;
        }
    }
    for (const auto& tEntry : tExclusions)
    {
        std::printf("  exclusion %-18s : %d\n", tEntry.first.c_str(), tEntry.second);
    }

    std::map<int, long long> tBase;
    std::map<int, long long> tOverlay;
    std::map<std::pair<int, int>, long long> tFlagsPlanes;
    double tMinLon = 1e9;
    double tMaxLon = -1e9;
    double tMinLat = 1e9;
    double tMaxLat = -1e9;
    double tMinElevation = 1e9;
    double tMaxElevation = -1e9;
    std::ofstream tDump;
    long long tDumped = 0;
    if (!tDumpPath.empty())
    {
        tDump.open(tDumpPath);
        tDump.precision(9);
    }
    const bool tOk = tReader.ForEachTriangle(
        [&](const DsfTriangle& pTriangle)
        {
            const bool tIsBase = (pTriangle.mPatchFlags & DsfReader::kPatchFlagOverlay) == 0;
            if (tDump.is_open() && tIsBase && pTriangle.mTerrain == tDumpTerrain)
            {
                for (int tCorner = 0; tCorner < 3; ++tCorner)
                {
                    tDump << pTriangle.mLon[tCorner] << ' ' << pTriangle.mLat[tCorner] << (tCorner < 2 ? ' ' : '\n');
                }
                ++tDumped;
            }
            ((pTriangle.mPatchFlags & DsfReader::kPatchFlagOverlay) ? tOverlay : tBase)[pTriangle.mTerrain]++;
            tFlagsPlanes[{pTriangle.mPatchFlags & 3, pTriangle.mPlaneCount}]++;
            for (int tCorner = 0; tCorner < 3; ++tCorner)
            {
                tMinLon = std::min(tMinLon, pTriangle.mLon[tCorner]);
                tMaxLon = std::max(tMaxLon, pTriangle.mLon[tCorner]);
                tMinLat = std::min(tMinLat, pTriangle.mLat[tCorner]);
                tMaxLat = std::max(tMaxLat, pTriangle.mLat[tCorner]);
                tMinElevation = std::min(tMinElevation, pTriangle.mElevation[tCorner]);
                tMaxElevation = std::max(tMaxElevation, pTriangle.mElevation[tCorner]);
            }
        },
        &tError);
    const double tTrianglesMs = ElapsedMs(&tClock);
    if (tDump.is_open())
    {
        std::printf("dumped %lld triangles of terrain %d to %s\n", tDumped, tDumpTerrain, tDumpPath.c_str());
    }
    std::printf("triangles: %s; lon %.4f..%.4f lat %.4f..%.4f elevation %.0f..%.0f m\n",
                tOk ? "ok" : tError.c_str(), tMinLon, tMaxLon, tMinLat, tMaxLat, tMinElevation, tMaxElevation);
    std::printf("patch flags x pool planes (1 = fiziksel, 2 = overlay):\n");
    for (const auto& tEntry : tFlagsPlanes)
    {
        std::printf("  flags %d planes %d : %8lld\n", tEntry.first.first, tEntry.first.second, tEntry.second);
    }
    for (size_t tIndex = 0; tIndex < tInfo.mTerrainNames.size(); ++tIndex)
    {
        const int tKey = static_cast<int>(tIndex);
        std::printf("  terrain %3d base %8lld overlay %8lld  %s\n", tKey, tBase.count(tKey) ? tBase[tKey] : 0LL,
                    tOverlay.count(tKey) ? tOverlay[tKey] : 0LL, tInfo.mTerrainNames[tIndex].c_str());
    }

    // Overlay envanteri: tanim basina komut sayilari
    DsfCommandStatistics tStatistics;
    const bool tCountOk = tReader.CountCommands(&tStatistics, &tError);
    const double tCountMs = ElapsedMs(&tClock);
    std::printf("commands: %s; terrain patches %lld\n", tCountOk ? "ok" : tError.c_str(),
                tStatistics.mTerrainPatches);
    long long tObjectTotal = 0;
    for (size_t tIndex = 0; tIndex < tInfo.mObjectNames.size(); ++tIndex)
    {
        tObjectTotal += tStatistics.mObjects[tIndex];
    }
    std::printf("objects: %d definitions, %lld placements\n", static_cast<int>(tInfo.mObjectNames.size()),
                tObjectTotal);
    for (size_t tIndex = 0; tIndex < tInfo.mObjectNames.size(); ++tIndex)
    {
        std::printf("  object  %4d %8lld  %s\n", static_cast<int>(tIndex), tStatistics.mObjects[tIndex],
                    tInfo.mObjectNames[tIndex].c_str());
    }
    std::printf("polygons: %d definitions\n", static_cast<int>(tInfo.mPolygonNames.size()));
    for (size_t tIndex = 0; tIndex < tInfo.mPolygonNames.size(); ++tIndex)
    {
        const DsfCommandStatistics::PolygonCount& tCount = tStatistics.mPolygons[tIndex];
        std::printf("  polygon %4d %8lld polys %8lld windings %10lld points  %s\n", static_cast<int>(tIndex),
                    tCount.mPolygons, tCount.mWindings, tCount.mPoints, tInfo.mPolygonNames[tIndex].c_str());
    }
    std::printf("networks: %d definitions\n", static_cast<int>(tInfo.mNetworkNames.size()));
    for (size_t tIndex = 0; tIndex < tInfo.mNetworkNames.size(); ++tIndex)
    {
        std::printf("  network %4d  %s\n", static_cast<int>(tIndex), tInfo.mNetworkNames[tIndex].c_str());
    }
    std::map<std::pair<int, int>, DsfCommandStatistics::ChainCount> tChains(tStatistics.mChains.begin(),
                                                                            tStatistics.mChains.end());
    for (const auto& tEntry : tChains)
    {
        std::printf("  chains definition %d subtype %3d : %8lld chains %10lld points\n", tEntry.first.first,
                    tEntry.first.second, tEntry.second.mChains, tEntry.second.mPoints);
    }

    // Butun raster katmanlari: boyut ve deger araligi
    for (const std::string& tName : tReader.RasterNames())
    {
        DsfRaster tLayer;
        if (!tReader.ReadRaster(tName, &tLayer) || tLayer.mValues.empty())
        {
            std::printf("raster %s: unreadable\n", tName.c_str());
            continue;
        }
        const auto tRange = std::minmax_element(tLayer.mValues.begin(), tLayer.mValues.end());
        std::printf("raster %-12s %dx%d post_centric=%d values %.1f..%.1f\n", tName.c_str(), tLayer.mWidth,
                    tLayer.mHeight, tLayer.mPostCentric ? 1 : 0, *tRange.first, *tRange.second);
    }
    ElapsedMs(&tClock);

    // DEM okuma suresi (zemin iscisi her karoda bunu yapar)
    DsfRaster tElevation;
    const bool tHasElevation = tReader.ReadRaster("elevation", &tElevation);
    const double tRasterMs = ElapsedMs(&tClock);
    std::printf("elevation raster: %s %dx%d post_centric=%d\n", tHasElevation ? "yes" : "no", tElevation.mWidth,
                tElevation.mHeight, tElevation.mPostCentric ? 1 : 0);
    std::printf("timing ms: open %.0f, triangles %.0f, command count %.0f, elevation %.0f\n", tOpenMs,
                tTrianglesMs, tCountMs, tRasterMs);
    return (tOk && tCountOk) ? 0 : 1;
}
