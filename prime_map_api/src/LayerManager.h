// Katman listesi: cizim sirasi, ac/kapa ve gorunurlugun ayar dosyasina
// kaydi. X-Plane'e bagimli DEGIL.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Layer.h"

class Settings;

enum class LayerPass
{
    All,      // hepsi (onbelleksiz cizim, onizleme)
    Cached,   // onbellekteki harita resmine girenler (isOverlay() == false)
    Overlay,  // her karede resmin ustune cizilenler (isOverlay() == true)
};

class LayerManager
{
public:
    // Listenin sonuna (en uste) ekler; sahipligi alir
    Layer* add(std::unique_ptr<Layer> pLayer);

    int count() const;
    Layer* at(int pIndex) const;

    void updateAll(float pNow);

    // Gorunen katmanlarin PrepareFrame'i (her karede, cizimden once). Biri yeni veri aldiysa true.
    bool prepareAll();

    // Bu modun varsayilan gorunurlugu (katmanlar ve alt katmanlar; ayar dosyasi yok sayilir)
    void resetToDefaults(const XMapTheme& pTheme);

    // Durum imzasi: gorunurluk, alt katmanlar, ust katman (overlay) ayrimi. Degisirse harita
    // resmi hemen yeniden cizilir (XMapImageCache).
    uint64_t visibilitySignature() const;

    // Once gorunen katmanlarin geometrisi (liste sirasiyla), sonra yazilari
    // (Layer::LabelPriority sirasiyla)
    void drawAll(const drawContext& pContext, LayerPass pPass = LayerPass::All);

    // Imlecin altindaki seyin aciklamasi: en ustteki gorunen katmandan baslar
    std::string describeAt(const drawContext& pContext, double pX, double pY) const;
    // API sorgusu: imlecin altinda bir sey olan her gorunen katman (en ustteki once): kimlik + metin
    std::vector<std::pair<std::string, std::string>> describeAllAt(const drawContext& pContext, double pX,
                                                                   double pY) const;

    // Mod basina bolum ("layers.vfr" ...): anahtar = katman kimligi, deger = 1/0.
    void loadVisibility(const Settings& pSettings, const XMapTheme& pTheme);

    // Katmanin bu moddaki varsayilan gorunurlugu (temanin ayari, yoksa katmanin kendisi)
    static bool themeDefault(const Layer& pLayer, const XMapTheme& pTheme);
    void saveVisibility(Settings& pSettings, const XMapTheme& pTheme) const;

    // Alt katmanin ayar anahtari: "<katman>.<alt katman>" (ornek "airport_surfaces.boundary")
    static std::string subLayerKey(const Layer& pLayer, int pSubLayer);

    // Zoom araliklari (0.23): [layer_zoom] bolumu, anahtar = katman / alt katman kimligi,
    // deger = "<en az> <en cok>". Moddan bagimsiz. Dosyada olmayan katmanin siniri yoktur.
    void loadZoomRanges(const Settings& pSettings);
    void saveZoomRanges(Settings& pSettings) const;
    void resetZoomRanges();
    // Katmanlara o anki zoom'u verir (drawAll kendisi cagirir; sorgu / onizleme icin ayrica)
    void setCurrentZoom(double pZoom);

private:
    std::vector<std::unique_ptr<Layer>> mLayers;
};
