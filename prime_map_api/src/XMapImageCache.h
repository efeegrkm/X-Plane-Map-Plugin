// Harita resmi onbellegi: harita her karede bastan cizilmez, bir FBO dokusuna
// cizilip tek dortgen olarak ekrana basilir (max_fps hiz siniriyla).
#ifndef PRIME_MAP_SRC_XMAPIMAGECACHE_H
#define PRIME_MAP_SRC_XMAPIMAGECACHE_H

#include <functional>

#include "XMapView.h"
#include "ScreenMath.h"
#include "gfx/renderTarget.h"

namespace Gfx
{
class Canvas;
}

class XMapImageCache
{
public:
    // pView: resmin cizilecegi gorunum (genisletilmis viewport); pArea: onun boxel alani
    using renderFunction = std::function<void(const XMapView& pView, const RectD& pArea)>;

    void setMaxRate(double pRendersPerSecond);
    void invalidate();  // bir sonraki karede hemen yeniden ciz

    // Her karede, pencere cizilirken. pUrgent: hemen ciz; pDirty/pBusy: hiz sinirinda.
    // Resmi gerekirse yeniden cizip ekrana basar; basarisizsa false.
    bool draw(Gfx::Canvas& pCanvas, const XMapView& pView, const Affine2D& pBoxelToPixel, float pNow, bool pUrgent,
              bool pDirty, bool pBusy, const renderFunction& pRender);

    // Teshis: son tam saniyedeki yeniden cizim sayisi
    int rendersPerSecond() const;

private:
    void blit(Gfx::Canvas& pCanvas, const XMapView& pView, const Affine2D& pBoxelToPixel) const;
    // Eski resmin su anki gorunumde ekrandaki dortgeni (boxel)
    RectD screenRectOfImage(const XMapView& pView, const Affine2D& pBoxelToPixel) const;

    Gfx::renderTarget mTarget;
    double mMaxRate = 15.0;
    bool mValid = false;
    bool mPendingDirty = false;  // hiz sinirina takilan "yeni veri" isareti
    float mLastRender = -100.0f;

    // Resmin cizildigi gorunum
    double mCenterX = 0.0;  // Mercator biriminde
    double mCenterY = 0.0;
    double mWorldSize = 0.0;
    RectD mArea;            // dokunun kapladigi boxel alani
    RectD mWindowArea;      // o andaki pencere alani

    float mCountStart = 0.0f;
    int mCount = 0;
    int mLastCount = 0;
};
#endif  // PRIME_MAP_SRC_XMAPIMAGECACHE_H
