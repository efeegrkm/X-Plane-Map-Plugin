// Texture.h
// Doku yardimcilari: RGBA goruntuden doku olusturma ve enlem/boylam
// dikdortgenine oturtup cizme. OpenGL cagrilari sadece Texture.cpp icinde.
//
// X-Plane kurali: doku numarasi XPLMGenerateTextureNumbers ile alinir,
// baglamak icin XPLMBindTexture2d kullanilir (dogrudan glBindTexture degil).
#pragma once

#include <cstdint>

class MapView;

namespace Gfx
{

// pSize x pSize RGBA (satir 0 = kuzey) goruntuden doku. Doku numarasi doner.
// Ana thread'de, OpenGL baglami etkinken (draw callback icinde) cagrilmali.
int CreateTexture(int pSize, const uint8_t* pRgba);
void DeleteTexture(int pTexture);

// Dokuyu [pWest, pEast] x [pSouth, pNorth] alanina cizer. Doku enlem/boylam
// izgarasinda, harita Mercator: dokuyu pStrips yatay seride bolup her seridi
// kendi enleminde cizerek Mercator'un enleme gore uzamasini izleriz.
void DrawTextureLatLon(int pTexture, const MapView& pView, double pWest, double pSouth, double pEast, double pNorth,
                       int pStrips);

}  // namespace Gfx
