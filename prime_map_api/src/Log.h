// X-Plane Log.txt'ye satir yazar; std::format bicimi ({} / {:.4f}).
// Sadece ana thread'den cagrilir (XPLMDebugString arka plan thread'inde cagrilmamali).
#ifndef PRIME_MAP_SRC_LOG_H
#define PRIME_MAP_SRC_LOG_H

#include <format>
#include <functional>
#include <string_view>
#include <utility>

void logLine(std::string_view pText);

// Satir oneki (varsayilan "[prime-map]") ve istege bagli hedef (bos: Log.txt)
void setLogPrefix(std::string_view pPrefix);
void setLogCallback(std::function<void(std::string_view)> pCallback);

template <typename... TArgs>
void log(std::format_string<TArgs...> pFormat, TArgs&&... pArgs)
{
    logLine(std::format(pFormat, std::forward<TArgs>(pArgs)...));
}
#endif  // PRIME_MAP_SRC_LOG_H
