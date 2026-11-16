// prime::MapLayer enum'u ile katman / alt katman kimlikleri arasindaki tek esleme tablosu ve
// katman panelinin ac/kapa kurallari. X-Plane'e bagimli degil (testlerde calisir).
#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "PrimeMap.h"

class LayerManager;
class Layer;
struct MapTheme;

namespace LayerRules
{

// katman(alt katmansa sahibi) LayerManager'da; yoksa / grupsa nullptr
Layer* findLayer(const LayerManager& pLayers, prime::MapLayer pLayer);
// Alt katmanin sahibindeki indeksi; alt katman degilse -1
int subLayerIndex(const LayerManager& pLayers, prime::MapLayer pLayer);

bool isVisible(const LayerManager& pLayers, prime::MapLayer pLayer);
prime::LayerState state(const LayerManager& pLayers, prime::MapLayer pLayer);
// Degisiklik olduysa true
bool setVisible(LayerManager& pLayers, const MapTheme& pTheme, prime::MapLayer pLayer, bool pVisible);
bool toggle(LayerManager& pLayers, const MapTheme& pTheme, prime::MapLayer pLayer);
// Sadece verilenler acik kalir (ust ogeleri gerektikce acilir)
void showOnly(LayerManager& pLayers, const MapTheme& pTheme, const prime::MapLayer* pBegin,
              const prime::MapLayer* pEnd);

// Panel grubu adi ("Airports") -> grup ogesi
std::optional<prime::MapLayer> groupFromPanelName(std::string_view pName);

// --- Katman hiyerarsisi (tablo sirasi = panel sirasi) ---

std::optional<prime::MapLayer> parentOf(prime::MapLayer pLayer);  // grup / katmanin ust ogesi
std::vector<prime::MapLayer> childrenOf(prime::MapLayer pLayer);
std::vector<prime::MapLayer> topLevelLayers();
prime::LayerKind kindOf(prime::MapLayer pLayer);
const char* layerName(prime::MapLayer pLayer);
const char* layerKey(prime::MapLayer pLayer);  // ayar anahtari ("runways.water")
std::optional<prime::MapLayer> layerFromKey(std::string_view pKey);
bool isLayerDefaultVisible(prime::MapLayer pLayer, prime::MapMode pMode);

}  // namespace LayerRules
