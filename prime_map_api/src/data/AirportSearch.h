// Havalimani arama: kimlik, ICAO kodu ve ad eslestirmesi.
#ifndef PRIME_MAP_SRC_DATA_AIRPORTSEARCH_H
#define PRIME_MAP_SRC_DATA_AIRPORTSEARCH_H

#include <string_view>
#include <vector>

#include "Geo.h"

class AirportGeometryStore;
struct AirportGeometry;

std::vector<int> findAirports(const AirportGeometryStore& pStore, std::string_view pQuery, int pMaxResults);

double zoomToFitAirport(const AirportGeometry& pAirport, double pViewWidth, double pViewHeight);

LatLon airportCenter(const AirportGeometry& pAirport);
#endif
