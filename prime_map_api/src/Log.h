// Log.h
// X-Plane'in Log.txt dosyasina "[prime-map-plugin] ..." on ekiyle satir yazar.
// Bicim C++20 std::format ile: {} yer tutucu, {:.4f} ondalik hane.
// Kullanim: Log("point added: {:.4f} {:.4f}", tLat, tLon);
//
// Sadece ana thread'den cagrilir (XPLMDebugString arka plan thread'inde
// cagrilmamali; arka plan isleri rapor metni dondurur).
#pragma once

#include <format>
#include <functional>
#include <string_view>
#include <utility>

void LogLine(std::string_view pText);

// Satir oneki (varsayilan "[prime-map]") ve istege bagli hedef (bos: Log.txt)
void SetLogPrefix(std::string_view pPrefix);
void SetLogCallback(std::function<void(std::string_view)> pCallback);

template <typename... TArgs>
void Log(std::format_string<TArgs...> pFormat, TArgs&&... pArgs)
{
    LogLine(std::format(pFormat, std::forward<TArgs>(pArgs)...));
}
