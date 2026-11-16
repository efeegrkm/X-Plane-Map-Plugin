// Ekrana degen havalimanlarini bulur; merkeze yakin olan once (ayrintisi
// once okunsun). pNeedNodes true: sadece dugumu (yuzey/cizgi) olanlar;
// sadece helipad satiri olan heliport'larin dugumu yoktur.
// AirportSurfaceLayer, AirportObjectsLayer ve TaxiRouteLayer ortak kullanir.
#pragma once

#include <vector>

class AirportGeometryStore;
class XMapView;

void findVisibleAirports(const XMapView& pView, const AirportGeometryStore& pStore, std::vector<int>& pScratch,
                         std::vector<int>& pOut, bool pNeedNodes = true);
