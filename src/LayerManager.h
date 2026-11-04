// LayerManager.h
// Katman listesi: cizim sirasi, ac/kapa ve gorunurlugun ayar dosyasina
// kaydi. X-Plane'e bagimli DEGIL.
//
// Liste alttan uste dogru cizilir: ilk eklenen en altta kalir.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Layer.h"

class Settings;

class LayerManager
{
public:
    // Katmani listenin sonuna (en uste) ekler ve sahipligini alir
    Layer* Add(std::unique_ptr<Layer> pLayer);

    int Count() const;
    Layer* At(int pIndex) const;

    void UpdateAll(float pNow);

    // Once gorunen katmanlarin geometrisi (liste sirasiyla), sonra yazilari
    // (Layer::LabelPriority sirasiyla)
    void DrawAll(const DrawContext& pContext);

    // Imlecin altindaki seyin aciklamasi: en ustteki gorunen katmandan baslar
    std::string DescribeAt(const DrawContext& pContext, double pX, double pY) const;

    // Mod basina bolum ("layers.vfr" ...): anahtar = katman kimligi, deger = 1/0.
    // Dosyada yoksa: temanin varsayilani, o da yoksa katmanin kendi varsayilani.
    // Scenery icin eski [layers] bolumu de okunur (0.12 ve oncesinin ayari).
    void LoadVisibility(const Settings& pSettings, const MapTheme& pTheme);
    void SaveVisibility(Settings& pSettings, const MapTheme& pTheme) const;

private:
    std::vector<std::unique_ptr<Layer>> mLayers;
};
