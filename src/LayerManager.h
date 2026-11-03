// LayerManager.h
// Katman listesi: cizim sirasi, ac/kapa ve gorunurlugun ayar dosyasina
// kaydi. X-Plane'e bagimli DEGIL.
//
// Liste alttan uste dogru cizilir: ilk eklenen en altta kalir.
#pragma once

#include <memory>
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

    // Once gorunen katmanlarin geometrisi, sonra hepsinin yazilari
    void DrawAll(const DrawContext& pContext);

    // [layers] bolumu: anahtar = katman kimligi, deger = 1/0
    void LoadVisibility(const Settings& pSettings);
    void SaveVisibility(Settings& pSettings) const;

private:
    std::vector<std::unique_ptr<Layer>> mLayers;
};
