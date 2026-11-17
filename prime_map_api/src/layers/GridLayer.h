// Enlem/boylam izgarasi; aralik zoom'a gore seyreklestirilir (cizgiler cok
// yaklasirsa daha seyrek adim). Etiketler sol (enlem) ve alt (boylam) kenarda.
#ifndef PRIME_MAP_SRC_LAYERS_GRIDLAYER_H
#define PRIME_MAP_SRC_LAYERS_GRIDLAYER_H

#include "Layer.h"

class GridLayer : public Layer
{
public:
    GridLayer();

    void draw(const drawContext& pContext) override;
    void drawOverlay(const drawContext& pContext) override;  // kenar yazilari: her karede ustte

private:
    static double stepFor(const drawContext& pContext);
};
#endif  // PRIME_MAP_SRC_LAYERS_GRIDLAYER_H
