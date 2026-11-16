// API impl dosyalarinin paylastigi kucuk yardimcilar.
#ifndef PRIME_MAP_API_UTIL_H
#define PRIME_MAP_API_UTIL_H

#include <array>
#include <cctype>
#include <sstream>
#include <string>

#include "XPLMPlugin.h"
#include "XPLMUtilities.h"

#include "PrimeMap.h"

namespace prime::api
{

inline std::string hostPluginDirectory()
{
    // .../Resources/plugins/<host>/64/win.xpl -> .../Resources/plugins/<host>/
    std::array<char, 2048> tXplPath = {};
    XPLMGetPluginInfo(XPLMGetMyID(), nullptr, tXplPath.data(), nullptr, nullptr);
    std::string tPath = tXplPath.data();
    for (int tLevel = 0; tLevel < 2; ++tLevel)
    {
        const size_t tCut = tPath.find_last_of("/\\");
        if (tCut != std::string::npos)
        {
            tPath.erase(tCut);
        }
    }
    return tPath + XPLMGetDirectorySeparator();
}

inline std::string withSeparator(std::string pPath)
{
    if (!pPath.empty() && pPath.back() != '/' && pPath.back() != '\\')
    {
        pPath += '/';
    }
    return pPath;
}

inline std::string upper(std::string pText)
{
    for (char& tChar : pText)
    {
        tChar = static_cast<char>(std::toupper(static_cast<unsigned char>(tChar)));
    }
    return pText;
}

inline Rgb parseRgb(std::istringstream& pStream)
{
    Rgb tColor;
    pStream >> tColor.mR >> tColor.mG >> tColor.mB;
    return tColor;
}

}  // namespace prime::api

#endif  // PRIME_MAP_API_UTIL_H
