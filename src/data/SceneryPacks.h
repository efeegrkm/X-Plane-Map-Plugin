// SceneryPacks.h
// X-Plane'in sahne paketi sirasi (Custom Scenery/scenery_packs.ini).
// X-Plane'e bagimli DEGIL.
//
//   SCENERY_PACK Custom Scenery/LTFM Istanbul/     -> etkin, en ustteki en oncelikli
//   SCENERY_PACK_DISABLED Custom Scenery/Eski/     -> atlanir
//   SCENERY_PACK *GLOBAL_AIRPORTS*                 -> X-Plane 12 kisaltmasi
//   SCENERY_PACK D:/Manzara/Ortho/                 -> mutlak yol
#pragma once

#include <string>
#include <vector>

// Etkin paketlerin klasorleri (sonunda '/' ile), oncelik sirasiyla
std::vector<std::string> ParseSceneryPacks(const std::string& pIniText, const std::string& pXPlaneDirectory);

// <X-Plane>/Custom Scenery/scenery_packs.ini'yi okuyup ParseSceneryPacks'e verir
std::vector<std::string> ReadSceneryPacks(const std::string& pXPlaneDirectory);
