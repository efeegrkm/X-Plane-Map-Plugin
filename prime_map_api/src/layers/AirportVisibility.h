// Ekrana degen havalimanlarini bulur; merkeze yakin olan once (ayrintisi
// once okunsun). pNeedNodes true: sadece dugumu (yuzey/cizgi) olanlar;
// sadece helipad satiri olan heliport'larin dugumu yoktur.
// AirportSurfaceLayer, AirportObjectsLayer ve TaxiRouteLayer ortak kullanir.
#ifndef PRIME_MAP_SRC_LAYERS_AIRPORTVISIBILITY_H
#define PRIME_MAP_SRC_LAYERS_AIRPORTVISIBILITY_H

#include <vector>

class AirportGeometryStore;
class XMapView;

void findVisibleAirports(const XMapView& pView, const AirportGeometryStore& pStore, std::vector<int>& pScratch,
                         std::vector<int>& pOut, bool pNeedNodes = true);
#endif  // PRIME_MAP_SRC_LAYERS_AIRPORTVISIBILITY_H
