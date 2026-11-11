// LayerTable.h
// PrimeMap::MapLayer enum'u ile katman / alt katman kimlikleri arasindaki TEK esleme tablosu
// ve katman panelinin ac/kapa kurallari. X-Plane'e bagimli DEGIL (testlerde calisir).
//
//   MapLayer::RunwaysWater  <->  "runways.water"  (ayar anahtari, bugunkuyle ayni)
//   MapLayer::AirportsGroup <->  "group.airports" (gruplarin ayari yok; durumu katmanlardan)
//
// Kurallar (katman paneli ve API ayni fonksiyonlari cagirir):
//   - ust ogeyi kapatmak alt ogeleri gizler (alt ogelerin kendi durumu korunur)
//   - kapali katmanin bir alt katmanini acmak SADECE o alt katmani acar (katman "yarim")
//   - son alt katman kapaninca katman da kapanir
//   - butun alt katmanlari kapali bir katman acilinca hepsi acilir
//   - grup acmak sadece o modda varsayilan acik katmanlari acar (hic yoksa hepsini);
//     grup kutusuna tiklama: varsayilan aciklardan biri kapaliysa onlari acar, yoksa hepsini kapatir
#pragma once

#include <optional>
#include <string_view>

#include "api/PrimeMap.h"

class LayerManager;
class Layer;
struct MapTheme;

namespace LayerRules
{

// Katman (alt katmansa sahibi) LayerManager'da; yoksa / grupsa nullptr
Layer* FindLayer(const LayerManager& pLayers, PrimeMap::MapLayer pLayer);
// Alt katmanin sahibindeki indeksi; alt katman degilse -1
int SubLayerIndex(const LayerManager& pLayers, PrimeMap::MapLayer pLayer);

bool IsVisible(const LayerManager& pLayers, PrimeMap::MapLayer pLayer);
PrimeMap::LayerState State(const LayerManager& pLayers, PrimeMap::MapLayer pLayer);
// Degisiklik olduysa true
bool SetVisible(LayerManager& pLayers, const MapTheme& pTheme, PrimeMap::MapLayer pLayer, bool pVisible);
bool Toggle(LayerManager& pLayers, const MapTheme& pTheme, PrimeMap::MapLayer pLayer);
// Sadece verilenler acik kalir (ust ogeleri gerektikce acilir)
void ShowOnly(LayerManager& pLayers, const MapTheme& pTheme, const PrimeMap::MapLayer* pBegin,
              const PrimeMap::MapLayer* pEnd);

// Panel grubu adi ("Airports") -> grup ogesi
std::optional<PrimeMap::MapLayer> GroupFromPanelName(std::string_view pName);

}  // namespace LayerRules
