// Onizleme araci icin: nav verisini X-Plane SDK'si yerine dogrudan
// dosyalardan okur (earth_nav.dat, earth_fix.dat, apt.dat basliklari).
// Eklentinin kendisi bunu KULLANMAZ: eklentide veri SDK'dan gelir (NavLoader).
#ifndef PRIME_MAP_TOOLS_PREVIEW_FILENAVLOADER_H
#define PRIME_MAP_TOOLS_PREVIEW_FILENAVLOADER_H

#include <string>

class NavDatabase;

// pXPlaneDirectory: "C:/X-Plane 11/" gibi. Okunan kayit sayisini doner.
int LoadNavFromFiles(const std::string& pXPlaneDirectory, NavDatabase& pDatabase);
#endif  // PRIME_MAP_TOOLS_PREVIEW_FILENAVLOADER_H
