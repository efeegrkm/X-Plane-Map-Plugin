// LayerManager.cpp
#include "LayerManager.h"

#include <algorithm>
#include <format>
#include <sstream>
#include <utility>

#include "MapView.h"
#include "Settings.h"

namespace
{

const char* kLegacySection = "layers";  // 0.12 ve oncesi: tek bolum (Scenery'ye tasinir)
const char* kZoomSection = "layer_zoom";  // 0.23: katman / alt katman zoom araliklari

// "<en az> <en cok>" -> iki sayi; okunamazsa false
bool ParseZoomRange(const std::string& pText, double* pMin, double* pMax)
{
    std::istringstream tStream(pText);
    tStream.imbue(std::locale::classic());  // nokta ile (Turkce Windows'ta da)
    return static_cast<bool>(tStream >> *pMin >> *pMax) && *pMin <= *pMax;
}

std::string FormatZoomRange(double pMin, double pMax)
{
    return std::format("{:.2f} {:.2f}", pMin, pMax);
}

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

void LayerManager::ResetToDefaults(const MapTheme& pTheme)
{
    const Settings tEmpty;  // bos ayar: her katman ve alt katman tema / katman varsayilanini alir
    LoadVisibility(tEmpty, pTheme);
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
        tMix(static_cast<uint64_t>(tLayer->MinZoom() * 100.0));  // zoom araligi degisti: yeniden ciz
        tMix(static_cast<uint64_t>(tLayer->MaxZoom() * 100.0));
        for (int tSub = 0; tSub < tLayer->SubLayerCount(); ++tSub)
        {
            tMix(tLayer->IsSubLayerVisible(tSub) ? 1 : 0);
            tMix(static_cast<uint64_t>(tLayer->SubLayerAt(tSub).mMinZoom * 100.0));
            tMix(static_cast<uint64_t>(tLayer->SubLayerAt(tSub).mMaxZoom * 100.0));
        }
    }
    return tHash;
}

std::string LayerManager::DescribeAt(const DrawContext& pContext, double pX, double pY) const
{
    for (auto tIterator = mLayers.rbegin(); tIterator != mLayers.rend(); ++tIterator)
    {
        if ((*tIterator)->IsVisible() && (*tIterator)->IsInZoomRange())
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

std::vector<std::pair<std::string, std::string>> LayerManager::DescribeAllAt(const DrawContext& pContext,
                                                                           double pX, double pY) const
{
    std::vector<std::pair<std::string, std::string>> tFound;
    for (auto tIterator = mLayers.rbegin(); tIterator != mLayers.rend(); ++tIterator)
    {
        if ((*tIterator)->IsVisible() && (*tIterator)->IsInZoomRange())
        {
            std::string tText = (*tIterator)->Describe(pContext, pX, pY);
            if (!tText.empty())
            {
                tFound.emplace_back((*tIterator)->Id(), std::move(tText));
            }
        }
    }
    return tFound;
}

void LayerManager::SetCurrentZoom(double pZoom)
{
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        tLayer->SetCurrentZoom(pZoom);
    }
}

void LayerManager::ResetZoomRanges()
{
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        tLayer->SetZoomRange(Layer::kNoMinZoom, Layer::kNoMaxZoom);
        for (int tSub = 0; tSub < tLayer->SubLayerCount(); ++tSub)
        {
            tLayer->SetSubLayerZoomRange(tSub, Layer::kNoMinZoom, Layer::kNoMaxZoom);
        }
    }
}

void LayerManager::LoadZoomRanges(const Settings& pSettings)
{
    ResetZoomRanges();
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        double tMin = 0.0;
        double tMax = 0.0;
        if (ParseZoomRange(pSettings.GetString(kZoomSection, tLayer->Id(), ""), &tMin, &tMax))
        {
            tLayer->SetZoomRange(tMin, tMax);
        }
        for (int tSub = 0; tSub < tLayer->SubLayerCount(); ++tSub)
        {
            if (ParseZoomRange(pSettings.GetString(kZoomSection, SubLayerKey(*tLayer, tSub), ""), &tMin, &tMax))
            {
                tLayer->SetSubLayerZoomRange(tSub, tMin, tMax);
            }
        }
    }
}

void LayerManager::SaveZoomRanges(Settings& pSettings) const
{
    // Bolum her seferinde bastan yazilir: siniri kaldirilan katmanin satiri dosyada kalmasin
    pSettings.ClearSection(kZoomSection);
    auto tLimited = [](double pMin, double pMax)
    {
        return pMin > Layer::kNoMinZoom || pMax < Layer::kNoMaxZoom;
    };
    for (const std::unique_ptr<Layer>& tLayer : mLayers)
    {
        if (tLimited(tLayer->MinZoom(), tLayer->MaxZoom()))
        {
            pSettings.SetString(kZoomSection, tLayer->Id(), FormatZoomRange(tLayer->MinZoom(), tLayer->MaxZoom()));
        }
        for (int tSub = 0; tSub < tLayer->SubLayerCount(); ++tSub)
        {
            const Layer::SubLayer& tPart = tLayer->SubLayerAt(tSub);
            if (tLimited(tPart.mMinZoom, tPart.mMaxZoom))
            {
                pSettings.SetString(kZoomSection, SubLayerKey(*tLayer, tSub),
                                    FormatZoomRange(tPart.mMinZoom, tPart.mMaxZoom));
            }
        }
    }
}

void LayerManager::DrawAll(const DrawContext& pContext, LayerPass pPass)
{
    if (pContext.mView)
    {
        SetCurrentZoom(pContext.mView->Zoom());  // katman / alt katman zoom araliklari (0.23)
    }
    auto tInPass = [pPass](const Layer& pLayer)
    {
        if (!pLayer.IsVisible() || !pLayer.IsInZoomRange())
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
    // Kenara yapisik parcalar: resimde olmayan her gecis (dogrudan cizim ya da ust gecis) cizer
    if (pPass != LayerPass::Cached)
    {
        for (const std::unique_ptr<Layer>& tLayer : mLayers)
        {
            if (tLayer->IsVisible() && tLayer->IsInZoomRange() && !tLayer->IsOverlay())
            {
                tLayer->DrawOverlay(pContext);
            }
        }
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
