// AirportSearch.h
// "Go to airport" paneli icin havalimani arama (0.18). X-Plane'e bagimli DEGIL.
//
// Yazilan metin buyuk/kucuk harf farksiz su sirayla eslesir:
//   0. kimlik veya ICAO kodu tam esit        "KSEA"
//   1. kimlik veya ICAO kodu bununla basliyor "KSE" -> KSEA, KSEE ...
//   2. adin bir kelimesi bununla basliyor     "tacoma" -> Seattle-Tacoma Intl
//   3. ad bunu iceriyor                       (en az 3 harf)
// Ayni derecede: pistli kara havalimani once, sonra en uzun pisti uzun olan, sonra kimlik.
// ~37 bin havalimaninda dogrusal tarama (tus basina ~1-2 ms): indeks gerekmedi.
#pragma once

#include <string_view>
#include <vector>

#include "Geo.h"

class AirportGeometryStore;
struct AirportGeometry;

// En iyi pMaxResults eslesmenin AirportGeometryStore indeksleri (en iyi once). Bos metin: bos liste.
std::vector<int> FindAirports(const AirportGeometryStore& pStore, std::string_view pQuery, int pMaxResults);

// Havalimaninin haritada tam gorunecegi zoom (pViewWidth x pViewHeight boxel pencerede).
// Kapsayan kutu yoksa / cok kucukse yakin bir varsayilan; uzak / asiri yakin zoom'a kirpilir.
double ZoomToFitAirport(const AirportGeometry& pAirport, double pViewWidth, double pViewHeight);

// Haritanin ortalanacagi nokta: kapsayan kutunun ortasi (yoksa referans noktasi)
LatLon AirportCenter(const AirportGeometry& pAirport);
