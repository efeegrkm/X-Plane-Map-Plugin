// Log.h
// X-Plane'in Log.txt dosyasina "[HaritaPlugin] ..." on ekiyle satir yazar.
// Kullanim: Log("nokta eklendi: %.4f %.4f", tLat, tLon);
#pragma once

void Log(const char* pFormat, ...);
