// Log.cpp
#include "Log.h"

#include <cstdarg>
#include <cstdio>

#include "XPLMUtilities.h"  // XPLMDebugString

void Log(const char* pFormat, ...)
{
    char tText[1024];
    va_list tArgs;
    va_start(tArgs, pFormat);
    std::vsnprintf(tText, sizeof(tText), pFormat, tArgs);
    va_end(tArgs);

    char tLine[1100];
    std::snprintf(tLine, sizeof(tLine), "[MapPlugin] %s\n", tText);
    XPLMDebugString(tLine);  // Log.txt'ye yazar
}
