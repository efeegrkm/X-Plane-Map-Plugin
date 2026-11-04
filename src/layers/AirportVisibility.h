// AirportVisibility.h
// Ekrana degen havalimanlarini bulur; haritanin merkezine yakin olan once
// gelir (ayrintisi once okunsun diye). pNeedNodes true ise sadece dugumu
// (yuzey / cizgi) olanlar: yuzey ve taksi katmani. Nesne katmani false verir,
// cunku sadece helipad (102) satiri olan heliport'larin dugumu yoktur.
// AirportSurfaceLayer, AirportObjectsLayer ve TaxiRouteLayer ortak kullanir.
#pragma once

#include <vector>

class AirportGeometryStore;
class MapView;

void FindVisibleAirports(const MapView& pView, const AirportGeometryStore& pStore, std::vector<int>& pScratch,
                         std::vector<int>& pOut, bool pNeedNodes = true);
