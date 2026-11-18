#include "Log.h"

#include <functional>
#include <string>

#include "XPLMUtilities.h"

namespace
{
std::string gLogPrefix = "[prime-map]";
std::function<void(std::string_view)> gLogCallback;
}

void setLogPrefix(std::string_view pPrefix)
{
    gLogPrefix = pPrefix;
}

void setLogCallback(std::function<void(std::string_view)> pCallback)
{
    gLogCallback = std::move(pCallback);
}

void logLine(std::string_view pText)
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
    XPLMDebugString(tLine.c_str());
}
