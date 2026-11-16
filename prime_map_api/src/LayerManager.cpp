
#include "LayerManager.h"

#include <algorithm>
#include <format>
#include <sstream>
#include <utility>

#include "XMapView.h"
#include "Settings.h"

namespace
{

const char* kZoomSection = "layer_zoom";  // 0.23: katman / alt katman zoom araliklari

// "<en az> <en cok>" -> iki sayi; okunamazsa false
bool parseZoomRange(const std::string& pText, double* pMin, double* pMax)
{
    std::istringstream tStream(pText);
    tStream.imbue(std::locale::classic());  // nokta ile (Turkce Windows'ta da)
    return static_cast<bool>(tStream >> *pMin >> *pMax) && *pMin <= *pMax;
}

std::string formatZoomRange(double pMin, double pMax)
{
    return std::format("{:.2f} {:.2f}", pMin, pMax);
}

}  // namespace

Layer* LayerManager::add(std::unique_ptr<Layer> pLayer)
{
    mLayers.push_back(std::move(pLayer));
    return mLayers.back().get();
}

int LayerManager::count() const
{
    return static_cast<int>(mLayers.size());
}

Layer* LayerManager::at(int pIndex) const
{
    if (pIndex < 0 || pIndex >= count())
    {
        return nullptr;
    }
    return mLayers[pIndex].get();
}

void LayerManager::updateAll(float pNow)
{
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        tLayer->update(pNow);
    }
}

void LayerManager::resetToDefaults(const XMapTheme& pTheme)
{
    const Settings tEmpty;  // bos ayar: her katman ve alt katman tema / katman varsayilanini alir
    loadVisibility(tEmpty, pTheme);
}

bool LayerManager::prepareAll()
{
    bool tChanged = false;
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        if (tLayer->isVisible() && tLayer->prepareFrame())
        {
            tChanged = true;
        }
    }
    return tChanged;
}

uint64_t LayerManager::visibilitySignature() const
{
    // FNV-1a benzeri karisim: her katman icin gorunur, overlay ve alt katman bitleri
    uint64_t tHash = 1469598103934665603ull;
    auto tMix = [&tHash](uint64_t pValue)
    {
        tHash = (tHash ^ pValue) * 1099511628211ull;
    };
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        tMix(tLayer->isVisible() ? 1 : 0);
        tMix(tLayer->isOverlay() ? 1 : 0);
        tMix(static_cast<uint64_t>(tLayer->minZoom() * 100.0));  // zoom araligi degisti: yeniden ciz
        tMix(static_cast<uint64_t>(tLayer->maxZoom() * 100.0));
        for (int tSub = 0; tSub < tLayer->subLayerCount(); ++tSub)
        {
            tMix(tLayer->isSubLayerVisible(tSub) ? 1 : 0);
            tMix(static_cast<uint64_t>(tLayer->subLayerAt(tSub).mMinZoom * 100.0));
            tMix(static_cast<uint64_t>(tLayer->subLayerAt(tSub).mMaxZoom * 100.0));
        }
    }
    return tHash;
}

std::string LayerManager::describeAt(const drawContext& pContext, double pX, double pY) const
{
    for (auto tIterator = mLayers.rbegin(); tIterator != mLayers.rend(); ++tIterator)
    {
        if ((*tIterator)->isVisible() && (*tIterator)->isInZoomRange())
        {
            std::string tText = (*tIterator)->describe(pContext, pX, pY);
            if (!tText.empty())
            {
                return tText;
            }
        }
    }
    return "";
}

std::vector<std::pair<std::string, std::string>> LayerManager::describeAllAt(const drawContext& pContext,
                                                                           double pX, double pY) const
{
    std::vector<std::pair<std::string, std::string>> tFound;
    for (auto tIterator = mLayers.rbegin(); tIterator != mLayers.rend(); ++tIterator)
    {
        if ((*tIterator)->isVisible() && (*tIterator)->isInZoomRange())
        {
            std::string tText = (*tIterator)->describe(pContext, pX, pY);
            if (!tText.empty())
            {
                tFound.emplace_back((*tIterator)->id(), std::move(tText));
            }
        }
    }
    return tFound;
}

void LayerManager::setCurrentZoom(double pZoom)
{
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        tLayer->setCurrentZoom(pZoom);
    }
}

void LayerManager::resetZoomRanges()
{
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        tLayer->setZoomRange(Layer::kNoMinZoom, Layer::kNoMaxZoom);
        for (int tSub = 0; tSub < tLayer->subLayerCount(); ++tSub)
        {
            tLayer->setSubLayerZoomRange(tSub, Layer::kNoMinZoom, Layer::kNoMaxZoom);
        }
    }
}

void LayerManager::loadZoomRanges(const Settings& pSettings)
{
    resetZoomRanges();
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        double tMin = 0.0;
        double tMax = 0.0;
        if (parseZoomRange(pSettings.getString(kZoomSection, tLayer->id(), ""), &tMin, &tMax))
        {
            tLayer->setZoomRange(tMin, tMax);
        }
        for (int tSub = 0; tSub < tLayer->subLayerCount(); ++tSub)
        {
            if (parseZoomRange(pSettings.getString(kZoomSection, subLayerKey(*tLayer, tSub), ""), &tMin, &tMax))
            {
                tLayer->setSubLayerZoomRange(tSub, tMin, tMax);
            }
        }
    }
}

void LayerManager::saveZoomRanges(Settings& pSettings) const
{
    // Bolum her seferinde bastan yazilir: siniri kaldirilan katmanin satiri dosyada kalmasin
    pSettings.clearSection(kZoomSection);
    auto tLimited = [](double pMin, double pMax)
    {
        return pMin > Layer::kNoMinZoom || pMax < Layer::kNoMaxZoom;
    };
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        if (tLimited(tLayer->minZoom(), tLayer->maxZoom()))
        {
            pSettings.setString(kZoomSection, tLayer->id(), formatZoomRange(tLayer->minZoom(), tLayer->maxZoom()));
        }
        for (int tSub = 0; tSub < tLayer->subLayerCount(); ++tSub)
        {
            const Layer::SubLayer& tPart = tLayer->subLayerAt(tSub);
            if (tLimited(tPart.mMinZoom, tPart.mMaxZoom))
            {
                pSettings.setString(kZoomSection, subLayerKey(*tLayer, tSub),
                                    formatZoomRange(tPart.mMinZoom, tPart.mMaxZoom));
            }
        }
    }
}

void LayerManager::drawAll(const drawContext& pContext, LayerPass pPass)
{
    if (pContext.mView)
    {
        setCurrentZoom(pContext.mView->zoom());  // katman / alt katman zoom araliklari (0.23)
    }
    auto tInPass = [pPass](const Layer& pLayer)
    {
        if (!pLayer.isVisible() || !pLayer.isInZoomRange())
        {
            return false;
        }
        if (pPass == LayerPass::All)
        {
            return true;
        }
        return pLayer.isOverlay() == (pPass == LayerPass::Overlay);
    };
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        if (tInPass(*tLayer))
        {
            tLayer->draw(pContext);
        }
    }
    // Yazilar etiket onceligi sirasiyla (esit oncelikte liste sirasi korunur)
    std::vector<Layer*> tByPriority;
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        if (tInPass(*tLayer))
        {
            tByPriority.push_back(tLayer.get());
        }
    }
    std::stable_sort(tByPriority.begin(), tByPriority.end(),
                     [](const Layer* pFirst, const Layer* pSecond)
                     {
                         return pFirst->labelPriority() < pSecond->labelPriority();
                     });
    for (Layer* tLayer : tByPriority)
    {
        tLayer->drawLabels(pContext);
    }
    // Kenara yapisik parcalar: resimde olmayan her gecis (dogrudan cizim ya da ust gecis) cizer
    if (pPass != LayerPass::Cached)
    {
        for (const std::unique_ptr<Layer>& tLayer : mLayers)
        {
            if (tLayer->isVisible() && tLayer->isInZoomRange() && !tLayer->isOverlay())
            {
                tLayer->drawOverlay(pContext);
            }
        }
    }
}

void LayerManager::loadVisibility(const Settings& pSettings, const XMapTheme& pTheme)
{
    const std::string tSection(layerSectionFor(pTheme.mMode));
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        const bool tDefault = themeDefault(*tLayer, pTheme);
        tLayer->setVisible(pSettings.getBool(tSection, tLayer->id(), tDefault));

        // Alt katmanlar: "<katman>.<alt>" anahtari; tema varsayilani ayni tam anahtarla verilebilir
        for (int tSub = 0; tSub < tLayer->subLayerCount(); ++tSub)
        {
            const std::string tKey = subLayerKey(*tLayer, tSub);
            bool tSubDefault = tLayer->subLayerAt(tSub).mDefaultVisible;
            for (const LayerDefault& tOverride : pTheme.mLayerDefaults)
            {
                if (tOverride.mId && tKey == tOverride.mId)
                {
                    tSubDefault = tOverride.mVisible;
                }
            }
            tLayer->setSubLayerVisible(tSub, pSettings.getBool(tSection, tKey, tSubDefault));
        }
    }
}

bool LayerManager::themeDefault(const Layer& pLayer, const XMapTheme& pTheme)
{
    bool tDefault = pLayer.defaultVisible();
    for (const LayerDefault& tOverride : pTheme.mLayerDefaults)
    {
        if (tOverride.mId && pLayer.id() == tOverride.mId)
        {
            tDefault = tOverride.mVisible;
        }
    }
    return tDefault;
}

std::string LayerManager::subLayerKey(const Layer& pLayer, int pSubLayer)
{
    return pLayer.id() + "." + pLayer.subLayerAt(pSubLayer).mId;
}

void LayerManager::saveVisibility(Settings& pSettings, const XMapTheme& pTheme) const
{
    const std::string tSection(layerSectionFor(pTheme.mMode));
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        pSettings.setBool(tSection, tLayer->id(), tLayer->isVisible());
        for (int tSub = 0; tSub < tLayer->subLayerCount(); ++tSub)
        {
            pSettings.setBool(tSection, subLayerKey(*tLayer, tSub), tLayer->isSubLayerVisible(tSub));
        }
    }
}
