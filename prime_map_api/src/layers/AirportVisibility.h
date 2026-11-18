// Ekrandaki havalimanlarini merkeze yakinlik sirasiyla bulur.
#ifndef PRIME_MAP_SRC_LAYERS_AIRPORTVISIBILITY_H
#define PRIME_MAP_SRC_LAYERS_AIRPORTVISIBILITY_H

#include <vector>

class AirportGeometryStore;
class XMapView;

void findVisibleAirports(const XMapView& pView, const AirportGeometryStore& pStore, std::vector<int>& pScratch,
                         std::vector<int>& pOut, bool pNeedNodes = true);
#endif
