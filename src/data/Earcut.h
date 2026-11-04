// Earcut.h
// Delikli poligon ucgenleme. X-Plane'e bagimli DEGIL (testli).
//
// Mapbox'in "earcut" algoritmasinin bu projenin kod standardiyla yeniden
// yazilmis hali (orijinal: github.com/mapbox/earcut, ISC lisansi; lisans
// metni Earcut.cpp'nin basinda). Neden bu algoritma: gercek apt.dat
// verisinde kendini kesen yuzey halkalari var (KSEA'da 34 kesisme olan bir
// taksi yolu). Duz "kulak kirpma" boyle veride takiliyor; earcut ise
// takilinca (1) tekrar eden / dogrusal noktalari atip yeniden dener,
// (2) kucuk yerel kesismeleri "tedavi eder", (3) son care poligonu gecerli
// bir kosegenle ikiye bolup her parcayi ayri ucgenler.
#pragma once

#include <vector>

#include "data/AirportShapes.h"

namespace Earcut
{

// pVertices: tum noktalar. pOuter: dis sinirin indeksleri, pHoles: deliklerin
// indeksleri (yon fark etmez). Sonuc: 3'erli indeksler (bos olabilir).
std::vector<int> Triangulate(const std::vector<LocalPoint>& pVertices, const std::vector<int>& pOuter,
                             const std::vector<std::vector<int>>& pHoles);

}  // namespace Earcut
