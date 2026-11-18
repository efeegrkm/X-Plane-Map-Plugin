// Koordinat, mesafe ve olcek metinlerinin bicimlendirilmesi.
#ifndef PRIME_MAP_SRC_XMAPFORMAT_H
#define PRIME_MAP_SRC_XMAPFORMAT_H

#include <string>

#include "Geo.h"

std::string formatLatLon(const LatLon& pPoint);

std::string formatGridLabel(double pDegrees, bool pIsLatitude, double pStepDegrees);

double chooseGridStepDeg(double pBoxelsPerDegree, double pMinSpacing);

double chooseScaleBarNm(double pNmPerBoxel, double pMaxBoxels);
#endif
