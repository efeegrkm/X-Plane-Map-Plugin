// Prime Map API: X-Plane 11 haritasini baska bir eklentinin icine gomer (SHARED DLL).
// Tum metotlar X-Plane ana thread'inden cagrilmalir; gecersiz deger no-op + Log uyarisi, false / bos doner.
// Kullanim: XMapManagerFactory::createXMapManager() -> setXxx + initialize() -> addXMap() + setXxx
// + xmap->initialize(). Bu baslik paylastigim tek arayuzdur; tipler api/XMapTypes.h'tadir.
#ifndef PRIME_MAP_H
#define PRIME_MAP_H

#include "api/XMap.h"
#include "api/XMapManager.h"
#include "api/XMapManagerFactory.h"
#include "api/XMapTypes.h"

#endif  // PRIME_MAP_H
