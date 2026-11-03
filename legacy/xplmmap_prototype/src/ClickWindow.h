// ClickWindow.h
// Haritanin tam ustune oturan, gorunmez XPLM penceresi.
//
// Neden var: X-Plane'in harita katmanlari fare tiklamasi ALMIYOR, ama normal
// XPLM pencereleri aliyor. Isaretleme modunda bu pencereyi haritanin uzerine
// koyup tiklamalari biz yakaliyoruz. Gezinme modunda gizli; boylece harita
// normal sekilde kaydirilir, X-Plane'in onerdigi gibi tiklama "gecirmiyoruz".
//
// Sol tik  : nokta / kose ekler
// Sag tik  : yarim kalan rota/bolgeyi bitirir
// Tekerlek : haritaya gecer (isaretlerken zoom yapilabilsin)
#pragma once

#include "XPLMDisplay.h"

#include "ScreenMath.h"

namespace ClickWindow {

void Create();
void Destroy();
void SetVisible(bool visible);
bool IsVisible();
void SetRect(const RectI& boxels);  // sadece degistiyse uygular
XPLMWindowID Id();

}  // namespace ClickWindow
