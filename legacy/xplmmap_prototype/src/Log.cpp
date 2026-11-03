// Log.cpp
#include "Log.h"

#include <cstdarg>
#include <cstdio>

#include "XPLMUtilities.h"  // XPLMDebugString

void Log(const char* format, ...) {
    char text[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);

    char line[1100];
    std::snprintf(line, sizeof(line), "[HaritaPlugin] %s\n", text);
    XPLMDebugString(line);  // Log.txt'ye yazar
}
