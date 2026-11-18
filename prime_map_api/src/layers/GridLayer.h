// Enlem ve boylam izgarasi.
#ifndef PRIME_MAP_SRC_LAYERS_GRIDLAYER_H
#define PRIME_MAP_SRC_LAYERS_GRIDLAYER_H

#include "Layer.h"

class GridLayer : public Layer
{
public:
    GridLayer();

    void draw(const drawContext& pContext) override;
    void drawOverlay(const drawContext& pContext) override;

private:
    static double stepFor(const drawContext& pContext);
};
#endif
