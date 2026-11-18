// Katman listesi: cizim sirasi, gorunurluk, zoom araliklari ve bunlarin ayar dosyasina kaydi.
#ifndef PRIME_MAP_SRC_LAYERMANAGER_H
#define PRIME_MAP_SRC_LAYERMANAGER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Layer.h"

class Settings;

enum class LayerPass
{
    All,
    Cached,
    Overlay,
};

class LayerManager
{
public:
    Layer* add(std::unique_ptr<Layer> pLayer);

    int count() const;
    Layer* at(int pIndex) const;

    void updateAll(float pNow);

    bool prepareAll();

    void resetToDefaults(const XMapTheme& pTheme);

    uint64_t visibilitySignature() const;

    void drawAll(const drawContext& pContext, LayerPass pPass = LayerPass::All);

    std::string describeAt(const drawContext& pContext, double pX, double pY) const;
    std::vector<std::pair<std::string, std::string>> describeAllAt(const drawContext& pContext, double pX,
                                                                   double pY) const;

    void loadVisibility(const Settings& pSettings, const XMapTheme& pTheme);

    static bool themeDefault(const Layer& pLayer, const XMapTheme& pTheme);
    void saveVisibility(Settings& pSettings, const XMapTheme& pTheme) const;

    static std::string subLayerKey(const Layer& pLayer, int pSubLayer);

    void loadZoomRanges(const Settings& pSettings);
    void saveZoomRanges(Settings& pSettings) const;
    void resetZoomRanges();
    void setCurrentZoom(double pZoom);

private:
    std::vector<std::unique_ptr<Layer>> mLayers;
};
#endif
