// Pencere stillerinde harita resmini dokuda tutup degismeyen kareleri yeniden cizmeyen onbellek.
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
    using renderFunction = std::function<void(const XMapView& pView, const RectD& pArea)>;

    void setMaxRate(double pRendersPerSecond);
    void invalidate();

    bool draw(Gfx::Canvas& pCanvas, const XMapView& pView, const Affine2D& pBoxelToPixel, float pNow, bool pUrgent,
              bool pDirty, bool pBusy, const renderFunction& pRender);

    int rendersPerSecond() const;

private:
    void blit(Gfx::Canvas& pCanvas, const XMapView& pView, const Affine2D& pBoxelToPixel) const;
    RectD screenRectOfImage(const XMapView& pView, const Affine2D& pBoxelToPixel) const;

    Gfx::renderTarget mTarget;
    double mMaxRate = 15.0;
    bool mValid = false;
    bool mPendingDirty = false;
    float mLastRender = -100.0f;

    double mCenterX = 0.0;
    double mCenterY = 0.0;
    double mWorldSize = 0.0;
    RectD mArea;
    RectD mWindowArea;

    float mCountStart = 0.0f;
    int mCount = 0;
    int mLastCount = 0;
};
#endif
