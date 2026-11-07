// LayerManager.cpp
#include "LayerManager.h"

#include <algorithm>
#include <utility>

#include "Settings.h"

namespace
{

const char* kLegacySection = "layers";  // 0.12 ve oncesi: tek bolum (Scenery'ye tasinir)

}  // namespace

Layer* LayerManager::Add(std::unique_ptr<Layer> pLayer)
{
    mLayers.push_back(std::move(pLayer));
    return mLayers.back().get();
}

int LayerManager::Count() const
{
    return static_cast<int>(mLayers.size());
}

Layer* LayerManager::At(int pIndex) const
{
    if (pIndex < 0 || pIndex >= Count())
    {
        return nullptr;
    }
    return mLayers[pIndex].get();
}

void LayerManager::UpdateAll(float pNow)
{
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        tLayer->Update(pNow);
    }
}

bool LayerManager::PrepareAll()
{
    bool tChanged = false;
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        if (tLayer->IsVisible() && tLayer->PrepareFrame())
        {
            tChanged = true;
        }
    }
    return tChanged;
}

uint64_t LayerManager::VisibilitySignature() const
{
    // FNV-1a benzeri karisim: her katman icin gorunur, overlay ve alt katman bitleri
    uint64_t tHash = 1469598103934665603ull;
    auto tMix = [&tHash](uint64_t pValue)
    {
        tHash = (tHash ^ pValue) * 1099511628211ull;
    };
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        tMix(tLayer->IsVisible() ? 1 : 0);
        tMix(tLayer->IsOverlay() ? 1 : 0);
        for (int tSub = 0; tSub < tLayer->SubLayerCount(); ++tSub)
        {
            tMix(tLayer->IsSubLayerVisible(tSub) ? 1 : 0);
        }
    }
    return tHash;
}

std::string LayerManager::DescribeAt(const DrawContext& pContext, double pX, double pY) const
{
    for (auto tIterator = mLayers.rbegin(); tIterator != mLayers.rend(); ++tIterator)
    {
        if ((*tIterator)->IsVisible())
        {
            std::string tText = (*tIterator)->Describe(pContext, pX, pY);
            if (!tText.empty())
            {
                return tText;
            }
        }
    }
    return "";
}

void LayerManager::DrawAll(const DrawContext& pContext, LayerPass pPass)
{
    auto tInPass = [pPass](const Layer& pLayer)
    {
        if (!pLayer.IsVisible())
        {
            return false;
        }
        if (pPass == LayerPass::All)
        {
            return true;
        }
        return pLayer.IsOverlay() == (pPass == LayerPass::Overlay);
    };
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        if (tInPass(*tLayer))
        {
            tLayer->Draw(pContext);
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
                         return pFirst->LabelPriority() < pSecond->LabelPriority();
                     });
    for (Layer* tLayer : tByPriority)
    {
        tLayer->DrawLabels(pContext);
    }
}

void LayerManager::LoadVisibility(const Settings& pSettings, const MapTheme& pTheme)
{
    const std::string tSection(LayerSectionFor(pTheme.mMode));
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        bool tDefault = ThemeDefault(*tLayer, pTheme);
        if (pTheme.mMode == MapMode::Scenery && tDefault)
        {
            tDefault = pSettings.GetBool(kLegacySection, tLayer->Id(), tDefault);  // eski ayar
        }
        tLayer->SetVisible(pSettings.GetBool(tSection, tLayer->Id(), tDefault));

        // Alt katmanlar: "<katman>.<alt>" anahtari; tema varsayilani ayni tam anahtarla verilebilir
        for (int tSub = 0; tSub < tLayer->SubLayerCount(); ++tSub)
        {
            const std::string tKey = SubLayerKey(*tLayer, tSub);
            bool tSubDefault = tLayer->SubLayerAt(tSub).mDefaultVisible;
            for (const LayerDefault& tOverride : pTheme.mLayerDefaults)
            {
                if (tOverride.mId && tKey == tOverride.mId)
                {
                    tSubDefault = tOverride.mVisible;
                }
            }
            tLayer->SetSubLayerVisible(tSub, pSettings.GetBool(tSection, tKey, tSubDefault));
        }
    }
}

bool LayerManager::ThemeDefault(const Layer& pLayer, const MapTheme& pTheme)
{
    bool tDefault = pLayer.DefaultVisible();
    for (const LayerDefault& tOverride : pTheme.mLayerDefaults)
    {
        if (tOverride.mId && pLayer.Id() == tOverride.mId)
        {
            tDefault = tOverride.mVisible;
        }
    }
    return tDefault;
}

std::string LayerManager::SubLayerKey(const Layer& pLayer, int pSubLayer)
{
    return pLayer.Id() + "." + pLayer.SubLayerAt(pSubLayer).mId;
}

void LayerManager::SaveVisibility(Settings& pSettings, const MapTheme& pTheme) const
{
    const std::string tSection(LayerSectionFor(pTheme.mMode));
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        pSettings.SetBool(tSection, tLayer->Id(), tLayer->IsVisible());
        for (int tSub = 0; tSub < tLayer->SubLayerCount(); ++tSub)
        {
            pSettings.SetBool(tSection, SubLayerKey(*tLayer, tSub), tLayer->IsSubLayerVisible(tSub));
        }
    }
}
