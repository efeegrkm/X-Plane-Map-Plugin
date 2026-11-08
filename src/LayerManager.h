// LayerManager.h
// Katman listesi: cizim sirasi, ac/kapa ve gorunurlugun ayar dosyasina
// kaydi. X-Plane'e bagimli DEGIL.
//
// Liste alttan uste dogru cizilir: ilk eklenen en altta kalir.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Layer.h"

class Settings;

// Hangi katmanlar cizilsin (0.18, MapImageCache)
enum class LayerPass
{
    All,      // hepsi (onbelleksiz cizim, onizleme)
    Cached,   // onbellekteki harita resmine girenler (IsOverlay() == false)
    Overlay,  // her karede resmin ustune cizilenler (IsOverlay() == true)
};

class LayerManager
{
public:
    // Katmani listenin sonuna (en uste) ekler ve sahipligini alir
    Layer* Add(std::unique_ptr<Layer> pLayer);

    int Count() const;
    Layer* At(int pIndex) const;

    void UpdateAll(float pNow);

    // Gorunen katmanlarin PrepareFrame'i (her karede, cizimden once). Biri yeni veri aldiysa true.
    bool PrepareAll();

    // Bu modun varsayilan gorunurlugu (katmanlar ve alt katmanlar; ayar dosyasi yok sayilir)
    void ResetToDefaults(const MapTheme& pTheme);

    // Durum imzasi: gorunurluk, alt katmanlar, ust katman (overlay) ayrimi. Degisirse harita
    // resmi hemen yeniden cizilir (MapImageCache).
    uint64_t VisibilitySignature() const;

    // Once gorunen katmanlarin geometrisi (liste sirasiyla), sonra yazilari
    // (Layer::LabelPriority sirasiyla)
    void DrawAll(const DrawContext& pContext, LayerPass pPass = LayerPass::All);

    // Imlecin altindaki seyin aciklamasi: en ustteki gorunen katmandan baslar
    std::string DescribeAt(const DrawContext& pContext, double pX, double pY) const;
    // API sorgusu: imlecin altinda bir sey olan her gorunen katman (en ustteki once): kimlik + metin
    std::vector<std::pair<std::string, std::string>> DescribeAllAt(const DrawContext& pContext, double pX,
                                                                   double pY) const;

    // Mod basina bolum ("layers.vfr" ...): anahtar = katman kimligi, deger = 1/0.
    // Dosyada yoksa: temanin varsayilani, o da yoksa katmanin kendi varsayilani.
    // Scenery icin eski [layers] bolumu de okunur (0.12 ve oncesinin ayari).
    // Varsayilan kapali katmanlar (ornek Taxi routes) eski bolumden okunmaz: o surumlerde
    // grup kutusu onlari da aciyordu, yeni kurulumda kapali gelsinler.
    void LoadVisibility(const Settings& pSettings, const MapTheme& pTheme);

    // Katmanin bu moddaki varsayilan gorunurlugu (temanin ayari, yoksa katmanin kendisi)
    static bool ThemeDefault(const Layer& pLayer, const MapTheme& pTheme);
    void SaveVisibility(Settings& pSettings, const MapTheme& pTheme) const;

    // Alt katmanin ayar anahtari: "<katman>.<alt katman>" (ornek "airport_surfaces.boundary")
    static std::string SubLayerKey(const Layer& pLayer, int pSubLayer);

private:
    std::vector<std::unique_ptr<Layer>> mLayers;
};
