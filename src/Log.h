// Log.h
// X-Plane'in Log.txt dosyasina "[MapPlugin] ..." on ekiyle satir yazar.
// Bicim C++20 std::format ile: {} yer tutucu, {:.4f} ondalik hane.
// Kullanim: Log("point added: {:.4f} {:.4f}", tLat, tLon);
//
// Sadece ana thread'den cagrilir (XPLMDebugString arka plan thread'inde
// cagrilmamali; arka plan isleri rapor metni dondurur).
#pragma once

#include <format>
#include <string_view>
#include <utility>

void LogLine(std::string_view pText);

template <typename... TArgs>
void Log(std::format_string<TArgs...> pFormat, TArgs&&... pArgs)
{
    LogLine(std::format(pFormat, std::forward<TArgs>(pArgs)...));
}
