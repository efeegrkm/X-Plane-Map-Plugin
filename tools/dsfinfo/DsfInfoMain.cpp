// DsfInfoMain.cpp
// dsf_info: bir DSF dosyasinin ozetini yazar (gelistirme ve teshis icin).
//   dsf_info "<.../+47-123.dsf>"
// Ozellikler, arazi turu adlari, her arazi turunun ucgen sayisi (overlay ayri),
// koordinat araligi. 7z ile sikistirilmis dosyalari kendisi acar.
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "data/DsfReader.h"
#include "data/SevenZip.h"

int main(int pArgc, char** pArgv)
{
    if (pArgc < 2)
    {
        std::fprintf(stderr, "usage: dsf_info <file.dsf> [--raw out.dsf]\n");
        return 2;
    }
    std::ifstream tFile(pArgv[1], std::ios::binary);
    std::vector<uint8_t> tData((std::istreambuf_iterator<char>(tFile)), std::istreambuf_iterator<char>());
    std::printf("file: %s (%d bytes)\n", pArgv[1], static_cast<int>(tData.size()));

    std::string tError;
    if (SevenZip::IsArchive(tData.data(), tData.size()))
    {
        std::vector<uint8_t> tPlain;
        if (!SevenZip::ExtractFirstFile(tData, &tPlain, &tError))
        {
            std::printf("7z error: %s\n", tError.c_str());
            return 1;
        }
        std::printf("7z archive -> %d bytes\n", static_cast<int>(tPlain.size()));
        tData.swap(tPlain);
    }
    if (pArgc >= 4 && std::string(pArgv[2]) == "--raw")
    {
        // Acilmis DSF'i diske yaz (bicimi baska araclarla incelemek icin)
        std::ofstream tRaw(pArgv[3], std::ios::binary);
        tRaw.write(reinterpret_cast<const char*>(tData.data()), static_cast<std::streamsize>(tData.size()));
        std::printf("raw DSF written: %s\n", pArgv[3]);
    }

    DsfReader tReader;
    if (!tReader.Open(tData, &tError))
    {
        std::printf("DSF error: %s\n", tError.c_str());
        return 1;
    }
    const DsfInfo& tInfo = tReader.Info();
    std::printf("bounds: W %.3f S %.3f E %.3f N %.3f overlay=%d pools=%d\n", tInfo.mWest, tInfo.mSouth, tInfo.mEast,
                tInfo.mNorth, tInfo.mOverlay ? 1 : 0, tReader.PoolCount());
    for (const auto& tProperty : tInfo.mProperties)
    {
        if (tProperty.first.rfind("sim/", 0) == 0 && tProperty.first != "sim/exclude_obj")
        {
            std::printf("  prop %s = %s\n", tProperty.first.c_str(), tProperty.second.c_str());
        }
    }

    std::map<int, long long> tBase;
    std::map<int, long long> tOverlay;
    double tMinLon = 1e9;
    double tMaxLon = -1e9;
    double tMinLat = 1e9;
    double tMaxLat = -1e9;
    double tMinElevation = 1e9;
    double tMaxElevation = -1e9;
    const bool tOk = tReader.ForEachTriangle(
        [&](const DsfTriangle& pTriangle)
        {
            ((pTriangle.mPatchFlags & DsfReader::kPatchFlagOverlay) ? tOverlay : tBase)[pTriangle.mTerrain]++;
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
    std::printf("triangles: %s; lon %.4f..%.4f lat %.4f..%.4f elevation %.0f..%.0f m\n",
                tOk ? "ok" : tError.c_str(), tMinLon, tMaxLon, tMinLat, tMaxLat, tMinElevation, tMaxElevation);
    for (size_t tIndex = 0; tIndex < tInfo.mTerrainNames.size(); ++tIndex)
    {
        const int tKey = static_cast<int>(tIndex);
        std::printf("  terrain %3d base %8lld overlay %8lld  %s\n", tKey, tBase.count(tKey) ? tBase[tKey] : 0LL,
                    tOverlay.count(tKey) ? tOverlay[tKey] : 0LL, tInfo.mTerrainNames[tIndex].c_str());
    }
    return tOk ? 0 : 1;
}
