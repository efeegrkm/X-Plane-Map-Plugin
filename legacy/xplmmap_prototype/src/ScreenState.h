// ScreenState.h
// XPLM pencerelerinin koordinat sistemi (boxel) ile OpenGL pikselleri
// arasindaki donusumu tutar.
//
// Bu donusumu X-Plane bize dogrudan vermiyor. Kendi pencerelerimiz cizilirken
// OpenGL matrisleri "boxel -> ekran" seklinde kurulu oluyor; o an matrisleri
// okuyup sakliyoruz. UI olcekleme (%100, %150...) bu sayede kendiliginden
// hesaba katiliyor.
#pragma once

#include "ScreenMath.h"

namespace Screen {

// Bir XPLM penceresinin draw callback'i icinde cagrilmali
void CaptureWindowTransform(bool readFramebuffer);

bool HasWindowTransform();

// boxel <-> piksel
bool BoxelToPixel(double bx, double by, double* px, double* py);
bool PixelRectToBoxel(const RectD& pixels, RectD* boxels);

const Affine2D& BoxelToPixelAffine();
int WindowFramebuffer();  // teshis: pencere cizilirken bagli framebuffer (-1 = okunmadi)

}  // namespace Screen
