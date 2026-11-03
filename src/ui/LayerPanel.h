// LayerPanel.h
// "Layers" dugmesinin altinda acilan katman listesi (onay kutulari).
//
//   +------------------+
//   | LAYERS           |
//   | [x] Lat/lon grid |
//   | [x] Aircraft     |
//   +------------------+
//
// Panel sadece cizer ve hangi satira tiklandigini soyler; katmani acip
// kapatmak ve ayari kaydetmek MapWindow'un isi.
#pragma once

#include "ScreenMath.h"

class LayerManager;

class LayerPanel
{
public:
    bool IsOpen() const;
    void SetOpen(bool pOpen);
    void Toggle();

    // pAnchor: "Layers" dugmesi; panel onun hemen altina, sol kenari
    // hizali acilir. Her cizimden once cagrilir.
    void Layout(const RectI& pAnchor, const LayerManager& pLayers);

    void Draw(int pMouseX, int pMouseY, const LayerManager& pLayers) const;

    // (pX, pY) panelin ustundeyse true. Bir katman satirina denk geldiyse
    // pOutIndex o katmanin indeksi, degilse -1 olur.
    bool HitTest(int pX, int pY, int* pOutIndex) const;

private:
    RectI RowRect(int pIndex) const;

    bool mOpen = false;
    RectI mRect;
    int mRowCount = 0;
};
