// MapFormat.h
// Haritadaki yazi bicimleri ve "yuvarlak" aralik secimi (izgara araligi,
// olcek cubugu uzunlugu). X-Plane'e bagimli DEGIL; testlerde denenir.
// Ciktilar ASCII: XPLMDrawString derece isareti ve Turkce harf cizemez.
#pragma once

#include <string>

#include "Geo.h"

// "41.12345 N  28.12345 E"
std::string FormatLatLon(const LatLon& pPoint);

// Izgara etiketi. Aralik 1 derece veya buyukse "41N", degilse derece +
// dakika: "41 30N", dakikadan kucukse "41 30.5N".
std::string FormatGridLabel(double pDegrees, bool pIsLatitude, double pStepDegrees);

// Ekranda en az pMinSpacing boxel aralik birakan en kucuk "yuvarlak"
// izgara araligi (derece): 30, 20, 10, 5, 2, 1, 30', 20', 10', 5', 2', 1', 30".
double ChooseGridStepDeg(double pBoxelsPerDegree, double pMinSpacing);

// Uzunlugu pMaxBoxels'i gecmeyen en buyuk "yuvarlak" olcek cubugu (NM):
// 1, 2, 5 x 10^n.
double ChooseScaleBarNm(double pNmPerBoxel, double pMaxBoxels);
