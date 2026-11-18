// Bir localizer'in hizmet ettigi pisti ve yaklasma esigini bulur.
#ifndef PRIME_MAP_SRC_DATA_LOCALIZERMATCH_H
#define PRIME_MAP_SRC_DATA_LOCALIZERMATCH_H

#include "Geo.h"

class AirportGeometryStore;

bool findLocalizerThreshold(const LatLon& pAntenna, double pCourseTrue, const AirportGeometryStore& pRunways,
                            LatLon* pOutThreshold);
#endif
