// AirportVisibility.h
// Ekrana degen ve ayrintisi (dugumu) olan havalimanlarini bulur; haritanin
// merkezine yakin olan once gelir (ayrintisi once okunsun diye).
// AirportSurfaceLayer ve AirportObjectsLayer ortak kullanir.
#pragma once

#include <vector>

class AirportGeometryStore;
class MapView;

void FindVisibleAirports(const MapView& pView, const AirportGeometryStore& pStore, std::vector<int>& pScratch,
                         std::vector<int>& pOut);
