// Custom Scenery/scenery_packs.ini dosyasindan sahne paketi sirasini okur.
#ifndef PRIME_MAP_SRC_DATA_SCENERYPACKS_H
#define PRIME_MAP_SRC_DATA_SCENERYPACKS_H

#include <string>
#include <vector>

std::vector<std::string> parseSceneryPacks(const std::string& pIniText, const std::string& pXPlaneDirectory);

std::vector<std::string> readSceneryPacks(const std::string& pXPlaneDirectory);
#endif
