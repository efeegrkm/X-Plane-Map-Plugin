// MapLayer.h
// X-Plane'in kendi haritasina (XPLM_MAP_USER_INTERFACE) eklenen katmanimiz.
//
// X-Plane her karede katmanin callback'lerini cagirir:
//   draw  : OpenGL ile cizgi/poligon (rota, bolge, iz, imlec artisi)
//   icon  : X-Plane'in ikon fonksiyonu ile nokta ikonlari
//   label : X-Plane'in yazi fonksiyonu ile etiketler
// Her callback o anki "projeksiyonu" verir; lat/lon'u onunla harita
// koordinatina ceviririz. Bu yuzden zoom/pan icin ekstra is yoktur.
//
// Draw callback ayrica tiklama hesabi icin gereken bilgileri toplar:
// haritanin OpenGL matrisi ve ekrandaki dikdortgeni.
#pragma once

#include <string>

#include "Geo.h"
#include "ScreenMath.h"

namespace MapLayer {

// Son harita ciziminde toplanan ekran bilgisi
struct ScreenInfo {
    bool valid = false;        // matris okunabildi mi
    Affine2D mapToPixel;       // harita birimi -> piksel
    RectD boundsMap;           // haritanin gorunen alani (harita birimi)
    RectD mapRectPixels;       // haritanin ekrandaki dikdortgeni (piksel)
    bool rectFromScissor = false;  // dikdortgen X-Plane'in scissor kutusundan mi geldi
    bool rotated = false;      // harita donuk mu (track-up)
    int viewport[4] = {0, 0, 0, 0};
    int framebuffer = -1;      // teshis: harita cizilirken bagli framebuffer
    float drawTime = -1.0f;    // son cizim zamani (XPLMGetElapsedTime)
};

// iconPathRelative: X-Plane klasorune gore ikon PNG yolu
void Init(const std::string& iconPathRelative, bool iconExists);
void Shutdown();

const ScreenInfo& Info();

// Harita su an acik ve katmanimiz ciziliyor mu?
bool IsMapVisible();

// Tiklamayi (harita biriminde) kuyruga ekler. Lat/lon'a cevirme sadece harita
// callback'i icinde yapilabildigi icin bir sonraki cizimde islenir.
void QueueClick(double mapX, double mapY);

// Imlecin altindaki lat/lon (imlec haritanin uzerindeyse)
bool GetHover(LatLon* out);

void SetDemoRoute(const LatLon& from, const char* fromName, const LatLon& to, const char* toName);
void AddTrailPoint(const LatLon& p);
void ClearTrail();

void SetDiagnostics(bool on);

}  // namespace MapLayer
