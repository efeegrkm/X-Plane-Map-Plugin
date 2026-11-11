// GridLayer.h
// Enlem/boylam izgarasi. Aralik zoom'a gore secilir: cizgiler ekranda
// birbirine cok yaklasirsa daha seyrek bir araliga gecilir (30 dereceden
// 30 saniyeye kadar). Etiketler haritanin sol (enlem) ve alt (boylam)
// kenarina yazilir.
#pragma once

#include "Layer.h"

class GridLayer : public Layer
{
public:
    GridLayer();

    void Draw(const DrawContext& pContext) override;
    void DrawOverlay(const DrawContext& pContext) override;  // kenar yazilari: her karede ustte

private:
    static double StepFor(const DrawContext& pContext);
};
