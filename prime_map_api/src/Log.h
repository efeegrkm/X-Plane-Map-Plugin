// X-Plane Log.txt'ye onekli satir yazar ve host'un log geri cagrisini besler.
#ifndef PRIME_MAP_SRC_LOG_H
#define PRIME_MAP_SRC_LOG_H

#include <format>
#include <functional>
#include <string_view>
#include <utility>

void logLine(std::string_view pText);

void setLogPrefix(std::string_view pPrefix);
void setLogCallback(std::function<void(std::string_view)> pCallback);

template <typename... TArgs>
void log(std::format_string<TArgs...> pFormat, TArgs&&... pArgs)
{
    logLine(std::format(pFormat, std::forward<TArgs>(pArgs)...));
}
#endif
