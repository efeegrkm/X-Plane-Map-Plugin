// Log.h
// X-Plane'in Log.txt dosyasina "[HaritaPlugin] ..." on ekiyle satir yazar.
// Kullanim: Log("nokta eklendi: %.4f %.4f", lat, lon);
#pragma once

void Log(const char* format, ...);
