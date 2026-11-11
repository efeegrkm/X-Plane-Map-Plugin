// GlScreen.h
// Pencere cizilirken boxel -> piksel donusumunu okuma ve cizimi pencere
// alaniyla kirpma (scissor).
//
// Neden gerekli: XPLM pencereleri boxel ile calisir, glScissor ise gercek
// piksel ister. UI olcek %100 degilse ikisi farklidir. X-Plane bu donusumu
// dogrudan vermiyor, ama pencerenin draw callback'i icinde OpenGL matrisleri
// "boxel -> ekran" olarak kurulu oluyor; o an matrisleri okuyup cikariyoruz.
// Pencere ayri bir isletim sistemi penceresine alinmissa (pop-out) da ayni
// sekilde calisir, cunku her cizimde yeniden okunur.
#pragma once

#include <array>
#include <optional>

#include "ScreenMath.h"

// Su anki OpenGL matrislerinden boxel -> piksel donusumu.
// Sadece bir XPLM penceresinin draw callback'i icinde anlamlidir.
std::optional<Affine2D> ReadBoxelToPixel();

// Bir donusumun boxel basina piksel orani (UI olcek: 1.0, 1.5, 2.0 ...)
double PixelsPerBoxel(const Affine2D& pBoxelToPixel);

// Yasam suresi boyunca cizimi verilen boxel dikdortgeniyle kirpar; bitince
// eski scissor durumunu geri yukler. X-Plane zaten bir kirpma kutusu
// kullaniyorsa onunla kesisimi alinir (disina tasmayalim).
class ScissorGuard
{
public:
    ScissorGuard(const Affine2D& pBoxelToPixel, const RectD& pBoxels);
    ~ScissorGuard();

    ScissorGuard(const ScissorGuard&) = delete;
    ScissorGuard& operator=(const ScissorGuard&) = delete;

    const RectD& PixelRect() const;  // teshis icin: uygulanan kutu (piksel)

private:
    bool mWasEnabled = false;
    std::array<int, 4> mOldBox = {0, 0, 0, 0};
    RectD mPixelRect;
};
