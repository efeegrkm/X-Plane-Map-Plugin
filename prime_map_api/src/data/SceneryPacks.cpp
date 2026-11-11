// SceneryPacks.cpp
#include "data/SceneryPacks.h"

#include <fstream>
#include <sstream>

namespace
{

bool IsAbsolutePath(const std::string& pPath)
{
    return (pPath.size() > 1 && pPath[1] == ':') || (!pPath.empty() && (pPath[0] == '/' || pPath[0] == '\\'));
}

}  // namespace

std::vector<std::string> ParseSceneryPacks(const std::string& pIniText, const std::string& pXPlaneDirectory)
{
    std::vector<std::string> tPacks;
    std::istringstream tStream(pIniText);
    std::string tLine;
    const std::string tPrefix = "SCENERY_PACK ";  // sonundaki bosluk: _DISABLED satirlarini disarida birakir
    while (std::getline(tStream, tLine))
    {
        if (!tLine.empty() && tLine.back() == '\r')
        {
            tLine.pop_back();
        }
        if (tLine.compare(0, tPrefix.size(), tPrefix) != 0)
        {
            continue;
        }
        std::string tPack = tLine.substr(tPrefix.size());
        if (tPack == "*GLOBAL_AIRPORTS*")
        {
            tPack = "Custom Scenery/Global Airports/";  // X-Plane 12 kisaltmasi
        }
        if (!tPack.empty() && tPack.back() != '/' && tPack.back() != '\\')
        {
            tPack += '/';
        }
        tPacks.push_back(IsAbsolutePath(tPack) ? tPack : pXPlaneDirectory + tPack);
    }
    return tPacks;
}

std::vector<std::string> ReadSceneryPacks(const std::string& pXPlaneDirectory)
{
    std::ifstream tFile(pXPlaneDirectory + "Custom Scenery/scenery_packs.ini");
    if (!tFile)
    {
        return {};
    }
    std::ostringstream tText;
    tText << tFile.rdbuf();
    return ParseSceneryPacks(tText.str(), pXPlaneDirectory);
}
