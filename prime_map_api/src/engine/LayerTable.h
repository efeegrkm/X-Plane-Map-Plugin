// prime::XMapLayer enum'u ile katman / alt katman kimlikleri arasindaki tek esleme tablosu ve
// katman panelinin ac/kapa kurallari. X-Plane'e bagimli degil (testlerde calisir).
#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "api/XMapTypes.h"

class LayerManager;
class Layer;
struct XMapTheme;

namespace LayerRules
{

// katman(alt katmansa sahibi) LayerManager'da; yoksa / grupsa nullptr
Layer* findLayer(const LayerManager& pLayers, prime::XMapLayer pLayer);
// Alt katmanin sahibindeki indeksi; alt katman degilse -1
int subLayerIndex(const LayerManager& pLayers, prime::XMapLayer pLayer);

bool isVisible(const LayerManager& pLayers, prime::XMapLayer pLayer);
prime::LayerState state(const LayerManager& pLayers, prime::XMapLayer pLayer);
// Degisiklik olduysa true
bool setVisible(LayerManager& pLayers, const XMapTheme& pTheme, prime::XMapLayer pLayer, bool pVisible);
bool toggle(LayerManager& pLayers, const XMapTheme& pTheme, prime::XMapLayer pLayer);
// Sadece verilenler acik kalir (ust ogeleri gerektikce acilir)
void showOnly(LayerManager& pLayers, const XMapTheme& pTheme, const prime::XMapLayer* pBegin,
              const prime::XMapLayer* pEnd);

// Panel grubu adi ("Airports") -> grup ogesi
std::optional<prime::XMapLayer> groupFromPanelName(std::string_view pName);

// --- Katman hiyerarsisi (tablo sirasi = panel sirasi) ---

std::optional<prime::XMapLayer> parentOf(prime::XMapLayer pLayer);  // grup / katmanin ust ogesi
std::vector<prime::XMapLayer> childrenOf(prime::XMapLayer pLayer);
std::vector<prime::XMapLayer> topLevelLayers();
prime::LayerKind kindOf(prime::XMapLayer pLayer);
const char* layerName(prime::XMapLayer pLayer);
const char* layerKey(prime::XMapLayer pLayer);  // ayar anahtari ("runways.water")
std::optional<prime::XMapLayer> layerFromKey(std::string_view pKey);
bool isLayerDefaultVisible(prime::XMapLayer pLayer, prime::XMapMode pMode);

}  // namespace LayerRules
