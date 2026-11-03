// ButtonPanel.h
// Haritanin yanina yapisan buton paneli (XPLM penceresi, kendimiz ciziyoruz).
//
// Butonlar: Gezin, Nokta, Rota, Bolge | Bitir, Geri al, Temizle
// Altinda durum satirlari: mod, imlecin lat/lon'u, cizim sayisi, uyarilar.
#pragma once

#include "XPLMDisplay.h"

#include "ScreenMath.h"

namespace ButtonPanel {

void Create();
void Destroy();
void SetVisible(bool visible);
bool IsVisible();
void SetRect(const RectI& boxels);  // sadece degistiyse uygular
int Width();
int Height();
XPLMWindowID Id();

}  // namespace ButtonPanel
