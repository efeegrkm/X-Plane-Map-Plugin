// SceneryIndex.cpp
#include "data/SceneryIndex.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <system_error>

#include "data/SceneryPacks.h"

namespace
{

namespace Fs = std::filesystem;

int FloorTo10(int pValue)
{
    return static_cast<int>(std::floor(pValue / 10.0)) * 10;
}

int FolderKey(int pWest, int pSouth)
{
    return SceneryIndex::TileKey(FloorTo10(pWest), FloorTo10(pSouth));
}

}  // namespace

int SceneryIndex::TileKey(int pWest, int pSouth)
{
    return (pSouth + 90) * 360 + (pWest + 180);
}

std::string SceneryIndex::TileName(int pWest, int pSouth)
{
    char tName[16];
    std::snprintf(tName, sizeof(tName), "%+03d%+04d", pSouth, pWest);
    return tName;
}

std::string SceneryIndex::FolderName(int pWest, int pSouth)
{
    return TileName(FloorTo10(pWest), FloorTo10(pSouth));
}

bool SceneryIndex::ParseTileName(const std::string& pFileName, int* pOutWest, int* pOutSouth)
{
    // Bicim: isaret + 2 hane enlem, isaret + 3 hane boylam, ".dsf" (buyuk/kucuk harf)
    if (pFileName.size() != 11)
    {
        return false;
    }
    const char tLatSign = pFileName[0];
    const char tLonSign = pFileName[3];
    if ((tLatSign != '+' && tLatSign != '-') || (tLonSign != '+' && tLonSign != '-'))
    {
        return false;
    }
    std::string tExtension = pFileName.substr(7);
    std::transform(tExtension.begin(), tExtension.end(), tExtension.begin(),
                   [](char pChar)
                   {
                       return static_cast<char>(std::tolower(static_cast<unsigned char>(pChar)));
                   });
    if (tExtension != ".dsf")
    {
        return false;
    }
    for (int tIndex : {1, 2, 4, 5, 6})
    {
        if (!std::isdigit(static_cast<unsigned char>(pFileName[tIndex])))
        {
            return false;
        }
    }
    const int tLat = (pFileName[1] - '0') * 10 + (pFileName[2] - '0');
    const int tLon = (pFileName[4] - '0') * 100 + (pFileName[5] - '0') * 10 + (pFileName[6] - '0');
    *pOutSouth = (tLatSign == '-') ? -tLat : tLat;
    *pOutWest = (tLonSign == '-') ? -tLon : tLon;
    return *pOutSouth >= -90 && *pOutSouth < 90 && *pOutWest >= -180 && *pOutWest < 180;
}

void SceneryIndex::Build(const std::string& pXPlaneDirectory)
{
    std::vector<std::string> tGlobalPacks;
    std::error_code tError;
    const Fs::path tGlobalRoot = Fs::path(pXPlaneDirectory) / "Global Scenery";
    for (const Fs::directory_entry& tEntry : Fs::directory_iterator(tGlobalRoot, tError))
    {
        if (tEntry.is_directory(tError))
        {
            tGlobalPacks.push_back(tEntry.path().string() + "/");
        }
    }
    std::sort(tGlobalPacks.begin(), tGlobalPacks.end());
    BuildFromPacks(ReadSceneryPacks(pXPlaneDirectory), tGlobalPacks);
}

void SceneryIndex::BuildFromPacks(const std::vector<std::string>& pCustomPacks,
                                  const std::vector<std::string>& pGlobalPacks)
{
    mTiles.clear();
    mInstalledFolders.clear();
    mFileCount = 0;
    mPackCount = 0;
    for (const std::string& tPack : pCustomPacks)
    {
        ScanPack(tPack, false);
    }
    for (const std::string& tPack : pGlobalPacks)
    {
        ScanPack(tPack, true);
    }
}

void SceneryIndex::ScanPack(const std::string& pPackDirectory, bool pGlobal)
{
    // <paket>/Earth nav data/<10x10 klasor>/<karo>.dsf
    std::error_code tError;
    const Fs::path tNavData = Fs::path(pPackDirectory) / "Earth nav data";
    bool tAny = false;
    for (const Fs::directory_entry& tFolder : Fs::directory_iterator(tNavData, tError))
    {
        if (!tFolder.is_directory(tError))
        {
            continue;
        }
        int tFolderWest = 0;
        int tFolderSouth = 0;
        if (pGlobal && ParseTileName(tFolder.path().filename().string() + ".dsf", &tFolderWest, &tFolderSouth))
        {
            mInstalledFolders.insert(TileKey(tFolderWest, tFolderSouth));
        }
        for (const Fs::directory_entry& tFile : Fs::directory_iterator(tFolder.path(), tError))
        {
            int tWest = 0;
            int tSouth = 0;
            if (!tFile.is_regular_file(tError) || !ParseTileName(tFile.path().filename().string(), &tWest, &tSouth))
            {
                continue;
            }
            DsfCandidate tCandidate;
            tCandidate.mPath = tFile.path().string();
            tCandidate.mSize = static_cast<uint64_t>(tFile.file_size(tError));
            tCandidate.mModified = static_cast<int64_t>(tFile.last_write_time(tError).time_since_epoch().count());
            mTiles[TileKey(tWest, tSouth)].push_back(tCandidate);
            ++mFileCount;
            tAny = true;
        }
    }
    if (tAny)
    {
        ++mPackCount;
    }
}

const std::vector<DsfCandidate>* SceneryIndex::CandidatesFor(int pWest, int pSouth) const
{
    const auto tFound = mTiles.find(TileKey(pWest, pSouth));
    return tFound == mTiles.end() ? nullptr : &tFound->second;
}

TileCoverage SceneryIndex::CoverageOf(int pWest, int pSouth) const
{
    if (CandidatesFor(pWest, pSouth))
    {
        return TileCoverage::HasDsf;
    }
    return IsRegionInstalled(pWest, pSouth) ? TileCoverage::Ocean : TileCoverage::NoData;
}

bool SceneryIndex::IsRegionInstalled(int pWest, int pSouth) const
{
    return mInstalledFolders.count(FolderKey(pWest, pSouth)) > 0;
}

int SceneryIndex::TileCount() const
{
    return static_cast<int>(mTiles.size());
}

int SceneryIndex::FileCount() const
{
    return mFileCount;
}

int SceneryIndex::PackCount() const
{
    return mPackCount;
}
