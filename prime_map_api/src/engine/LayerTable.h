// XMapLayer ile katman kimlikleri arasindaki esleme, katman agaci ve yerlesik zoom esikleri.
#ifndef PRIME_MAP_SRC_ENGINE_LAYERTABLE_H
#define PRIME_MAP_SRC_ENGINE_LAYERTABLE_H

#include <optional>
#include <string_view>
#include <vector>

#include "api/XMapTypes.h"

class LayerManager;
class Layer;
struct XMapTheme;

namespace LayerRules
{
Layer* findLayer(const LayerManager& pLayers, prime::XMapLayer pLayer);
int subLayerIndex(const LayerManager& pLayers, prime::XMapLayer pLayer);

bool isVisible(const LayerManager& pLayers, prime::XMapLayer pLayer);
prime::LayerState state(const LayerManager& pLayers, prime::XMapLayer pLayer);
bool setVisible(LayerManager& pLayers, const XMapTheme& pTheme, prime::XMapLayer pLayer, bool pVisible);
bool toggle(LayerManager& pLayers, const XMapTheme& pTheme, prime::XMapLayer pLayer);
void showOnly(LayerManager& pLayers, const XMapTheme& pTheme, const prime::XMapLayer* pBegin,
              const prime::XMapLayer* pEnd);

std::optional<prime::XMapLayer> groupFromPanelName(std::string_view pName);

std::optional<prime::XMapLayer> parentOf(prime::XMapLayer pLayer);
std::vector<prime::XMapLayer> childrenOf(prime::XMapLayer pLayer);
std::vector<prime::XMapLayer> topLevelLayers();
prime::LayerKind kindOf(prime::XMapLayer pLayer);
const char* layerName(prime::XMapLayer pLayer);
const char* layerKey(prime::XMapLayer pLayer);
std::optional<prime::XMapLayer> layerFromKey(std::string_view pKey);
bool isLayerDefaultVisible(prime::XMapLayer pLayer, prime::XMapMode pMode);

struct LayerZoomDefaults
{
    double mMinZoom;
    double mMaxZoom;
    double mHardMinZoom;
};

LayerZoomDefaults zoomDefaultsOf(prime::XMapLayer pLayer);

void applyBuiltInZoomRanges(LayerManager& pLayers);
}
#endif
