// LayerManager.cpp
#include "LayerManager.h"

#include <utility>

#include "Settings.h"

namespace
{

const char* kSection = "layers";

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

void LayerManager::DrawAll(const DrawContext& pContext)
{
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        if (tLayer->IsVisible())
        {
            tLayer->Draw(pContext);
        }
    }
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        if (tLayer->IsVisible())
        {
            tLayer->DrawLabels(pContext);
        }
    }
}

void LayerManager::LoadVisibility(const Settings& pSettings)
{
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        // Dosyada yoksa katmanin varsayilani kalir
        tLayer->SetVisible(pSettings.GetBool(kSection, tLayer->Id(), tLayer->IsVisible()));
    }
}

void LayerManager::SaveVisibility(Settings& pSettings) const
{
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        pSettings.SetBool(kSection, tLayer->Id(), tLayer->IsVisible());
    }
}
