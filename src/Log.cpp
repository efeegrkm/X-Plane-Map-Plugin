// Log.cpp
#include "Log.h"

#include <string>

#include "XPLMUtilities.h"  // XPLMDebugString

void LogLine(std::string_view pText)
{
    std::string tLine = "[prime-map-plugin] ";
    tLine += pText;
    tLine += '\n';
    XPLMDebugString(tLine.c_str());  // Log.txt'ye yazar
}
