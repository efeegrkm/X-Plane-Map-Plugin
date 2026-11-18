// Onizleme araci icin nav verisini X-Plane dosyalarindan okur.
#ifndef PRIME_MAP_TOOLS_PREVIEW_FILENAVLOADER_H
#define PRIME_MAP_TOOLS_PREVIEW_FILENAVLOADER_H

#include <string>

class NavDatabase;

int LoadNavFromFiles(const std::string& pXPlaneDirectory, NavDatabase& pDatabase);
#endif
