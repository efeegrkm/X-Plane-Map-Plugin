// Bir localizer'in (ILS/LOC) hizmet ettigi pisti bulur. X-Plane'e bagimli DEGIL.
// Neden: SDK localizer'in sadece anten yerini/yonunu verir; anten pistin OBUR
// ucunun biraz otesindedir, yaklasma "tuyu" esikten baslamali. Esik kurali:
//   - pistin inis yonu localizer yonuyle uyusmali (en fazla kMaxCourseDiff),
//   - anten, pistin karsi ucuna yakin ve pist ekseni uzerinde olmali.
// LDA/SDF gibi eksenden kayik localizer'lar eslesmez (cagiran anteni kullanir).
#pragma once

#include "Geo.h"

class AirportGeometryStore;

// Bulunduysa true; pOutThreshold'a inis yapilan pist ucunu yazar.
bool findLocalizerThreshold(const LatLon& pAntenna, double pCourseTrue, const AirportGeometryStore& pRunways,
                            LatLon* pOutThreshold);
