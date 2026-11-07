// MapImageCache.h
// Harita resmi onbellegi (0.18). Harita her karede bastan cizilmez: bir FBO
// dokusuna cizilir, her karede o doku tek bir dortgen olarak ekrana basilir.
//
//   +----------------------------------+   doku pencereden buyuk (her yanda ~%25 pay):
//   |   pay                            |   kaydirirken / ucak takibinde kenarlar bos kalmaz
//   |    +------------------------+    |
//   |    |  pencere               |    |   gorunum degisince eski resim KAYDIRILIR / OLCEKLENIR
//   |    |                        |    |   (tam piksele yuvarlanarak: bulaniklik yok), yeni resim
//   |    +------------------------+    |   en cok max_fps (15) hizinda cizilir
//   +----------------------------------+
//
// Ne zaman yeniden cizilir:
//   hemen     : ilk kez, pencere / UI olcegi degisti, katman / mod / cizim degisti (pUrgent),
//               eski resim pencereyi artik kaplamiyor (hizli kaydirma, cok zoom out)
//   hiz sinirinda (max_fps): gorunum degisti, yeni veri geldi (pDirty), veri yukleniyor (pBusy)
//   guvenlik  : en gec 1 saniyede bir (kacan bir "degisti" sinyali kalici olmasin)
//
// Etiketler pencerenin bos alaninda (ClearArea) yerlestirilir: paydaki yerlerde etiket
// yok; kaydirinca bir sonraki cizimde (<= 1/15 s) yerlerine gelirler.
// Ucak, ucus izi, yarim cizim (Layer::IsOverlay) resme girmez, her karede ustune cizilir.
// FBO kurulamazsa Draw false doner; cagiran haritayi eskisi gibi dogrudan cizer.
#pragma once

#include <functional>

#include "MapView.h"
#include "ScreenMath.h"
#include "gfx/RenderTarget.h"

namespace Gfx
{
class Canvas;
}

class MapImageCache
{
public:
    // pView: resmin cizilecegi gorunum (genisletilmis viewport); pArea: onun boxel alani
    using RenderFunction = std::function<void(const MapView& pView, const RectD& pArea)>;

    void SetMaxRate(double pRendersPerSecond);
    void Invalidate();  // bir sonraki karede hemen yeniden ciz

    // Her karede, pencere cizilirken. pView: gecerli gorunum (viewport = pencere alani).
    // pUrgent: hemen yeniden ciz; pDirty / pBusy: hiz sinirinda yeniden ciz.
    // Resmi (gerekirse once yeniden cizip) ekrana basar. Basarisizsa false.
    bool Draw(Gfx::Canvas& pCanvas, const MapView& pView, const Affine2D& pBoxelToPixel, float pNow, bool pUrgent,
              bool pDirty, bool pBusy, const RenderFunction& pRender);

    // Teshis: son tam saniyedeki yeniden cizim sayisi
    int RendersPerSecond() const;

private:
    void Blit(Gfx::Canvas& pCanvas, const MapView& pView, const Affine2D& pBoxelToPixel) const;
    // Eski resmin su anki gorunumde ekrandaki dortgeni (boxel)
    RectD ScreenRectOfImage(const MapView& pView, const Affine2D& pBoxelToPixel) const;

    Gfx::RenderTarget mTarget;
    double mMaxRate = 15.0;
    bool mValid = false;
    bool mPendingDirty = false;  // hiz sinirina takilan "yeni veri" isareti
    float mLastRender = -100.0f;

    // Resmin cizildigi gorunum
    double mCenterX = 0.0;  // Mercator (MapImageCache::Draw aciklamasi: pencere ortasina gore duzeltilmis)
    double mCenterY = 0.0;
    double mWorldSize = 0.0;
    RectD mArea;            // dokunun kapladigi boxel alani
    RectD mWindowArea;      // o andaki pencere alani

    // Teshis
    float mCountStart = 0.0f;
    int mCount = 0;
    int mLastCount = 0;
};
