// Log.cpp
// Log satirlari X-Plane'in Log.txt dosyasina (XPLMDebugString) ya da host'un verdigi
// callback'e gider. Onek varsayilan "[prime-map]"; API InitConfig::mLogPrefix ile degisir.
// Bu degiskenler kutuphanenin kopyasina (her eklenti DLL'ine) aittir.
#include "Log.h"

#include <functional>
#include <string>

#include "XPLMUtilities.h"  // XPLMDebugString

namespace
{

std::string gLogPrefix = "[prime-map]";
std::function<void(std::string_view)> gLogCallback;

}  // namespace

void SetLogPrefix(std::string_view pPrefix)
{
    gLogPrefix = pPrefix;
}

void SetLogCallback(std::function<void(std::string_view)> pCallback)
{
    gLogCallback = std::move(pCallback);
}

void LogLine(std::string_view pText)
{
    std::string tLine = gLogPrefix;
    tLine += ' ';
    tLine += pText;
    if (gLogCallback)
    {
        gLogCallback(tLine);
        return;
    }
    tLine += '\n';
    XPLMDebugString(tLine.c_str());  // Log.txt'ye yazar
}
