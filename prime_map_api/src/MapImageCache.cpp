#include "MapImageCache.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "gfx/Canvas.h"
#include "gfx/GlScreen.h"

namespace
{

const double kMarginFraction = 0.25;  // dokunun pencereden her yandaki payi (pencere boyunun orani)
const float kRefreshSeconds = 1.0f;   // en gec bu kadar surede bir yeniden cizim (guvenlik)
const int kMaxTextureSide = 8192;     // dokunun en buyuk kenari (piksel); asilirsa pay kuculur

bool sameRect(const RectD& pFirst, const RectD& pSecond)
{
    const double tEpsilon = 1e-6;
    return std::abs(pFirst.mLeft - pSecond.mLeft) < tEpsilon && std::abs(pFirst.mRight - pSecond.mRight) < tEpsilon &&
           std::abs(pFirst.mBottom - pSecond.mBottom) < tEpsilon && std::abs(pFirst.mTop - pSecond.mTop) < tEpsilon;
}

// Mercator x farki: 180. boylam sarilmasiyla en kisa yon
double wrapDelta(double pDelta)
{
    if (pDelta > 0.5)
    {
        return pDelta - 1.0;
    }
    if (pDelta < -0.5)
    {
        return pDelta + 1.0;
    }
    return pDelta;
}

}  // namespace

void XMapImageCache::setMaxRate(double pRendersPerSecond)
{
    mMaxRate = std::clamp(pRendersPerSecond, 1.0, 120.0);
}

void XMapImageCache::invalidate()
{
    mValid = false;
}

int XMapImageCache::rendersPerSecond() const
{
    return mLastCount;
}

RectD XMapImageCache::screenRectOfImage(const XMapView& pView, const Affine2D& pBoxelToPixel) const
{
    // Resimdeki bir nokta p (boxel) -> ekran: M + (c0 - c) * W + (p - M0) * W / W0
    const RectD& tViewport = pView.viewport();
    const double tWorld = pView.worldSize();
    const double tScale = tWorld / mWorldSize;
    const double tMidX = (tViewport.mLeft + tViewport.mRight) * 0.5;
    const double tMidY = (tViewport.mBottom + tViewport.mTop) * 0.5;
    const double tOldMidX = (mArea.mLeft + mArea.mRight) * 0.5;
    const double tOldMidY = (mArea.mBottom + mArea.mTop) * 0.5;
    double tShiftX = tMidX + wrapDelta(mCenterX - pView.centerMercatorX()) * tWorld - tOldMidX * tScale;
    double tShiftY = tMidY + (mCenterY - pView.centerMercatorY()) * tWorld - tOldMidY * tScale;
    if (std::abs(tScale - 1.0) < 1e-9)
    {
        // Sadece kaydirma: tam piksele yuvarla (doku pikselleri ekran pikseline oturur, bulaniklik yok)
        const double tPixelsPerBoxel = pixelsPerBoxel(pBoxelToPixel);
        tShiftX = std::round(tShiftX * tPixelsPerBoxel) / tPixelsPerBoxel;
        tShiftY = std::round(tShiftY * tPixelsPerBoxel) / tPixelsPerBoxel;
    }
    RectD tRect;
    tRect.mLeft = mArea.mLeft * tScale + tShiftX;
    tRect.mRight = mArea.mRight * tScale + tShiftX;
    tRect.mBottom = mArea.mBottom * tScale + tShiftY;
    tRect.mTop = mArea.mTop * tScale + tShiftY;
    return tRect;
}

bool XMapImageCache::draw(Gfx::Canvas& pCanvas, const XMapView& pView, const Affine2D& pBoxelToPixel, float pNow,
                         bool pUrgent, bool pDirty, bool pBusy, const renderFunction& pRender)
{
    if (pNow - mCountStart >= 1.0f)
    {
        mLastCount = mCount;
        mCount = 0;
        mCountStart = pNow;
    }

    // Pencerenin piksel dikdortgeni ve payli doku boyu (tam piksel)
    const RectD& tWindow = pView.viewport();
    const RectD tWindowPixels = transformRectBBox(pBoxelToPixel, tWindow);
    const int tMarginX = static_cast<int>(std::lround(tWindowPixels.width() * kMarginFraction));
    const int tMarginY = static_cast<int>(std::lround(tWindowPixels.height() * kMarginFraction));
    RectD tPixels;
    tPixels.mLeft = std::floor(tWindowPixels.mLeft) - tMarginX;
    tPixels.mRight = std::ceil(tWindowPixels.mRight) + tMarginX;
    tPixels.mBottom = std::floor(tWindowPixels.mBottom) - tMarginY;
    tPixels.mTop = std::ceil(tWindowPixels.mTop) + tMarginY;
    const int tWidth = static_cast<int>(tPixels.width());
    const int tHeight = static_cast<int>(tPixels.height());
    Affine2D tPixelToBoxel;
    if (tWidth <= 0 || tHeight <= 0 || tWidth > kMaxTextureSide || tHeight > kMaxTextureSide ||
        !invert(pBoxelToPixel, &tPixelToBoxel))
    {
        return false;
    }

    // Eski resmin pencereyi kaplayip kaplamadigina bakilir
    const bool tSizeChanged =
        tWidth != mTarget.width() || tHeight != mTarget.height() || !sameRect(tWindow, mWindowArea);
    bool tCovers = false;
    bool tViewChanged = true;
    if (mValid && !tSizeChanged)
    {
        const RectD tImage = screenRectOfImage(pView, pBoxelToPixel);
        const double tEpsilon = 0.01;
        tCovers = tImage.mLeft <= tWindow.mLeft + tEpsilon && tImage.mRight >= tWindow.mRight - tEpsilon &&
                  tImage.mBottom <= tWindow.mBottom + tEpsilon && tImage.mTop >= tWindow.mTop - tEpsilon;
        tViewChanged = !sameRect(tImage, mArea);
    }
    mPendingDirty = mPendingDirty || pDirty;

    const float tSinceLast = pNow - mLastRender;
    const bool tRateAllows = tSinceLast >= 1.0f / static_cast<float>(mMaxRate);
    const bool tRender = !mValid || tSizeChanged || pUrgent || !tCovers || tSinceLast >= kRefreshSeconds ||
                         (tRateAllows && (tViewChanged || mPendingDirty || pBusy));

    if (tRender)
    {
        if (mTarget.prepare(tWidth, tHeight) == 0)
        {
            mValid = false;
            return false;
        }
        const RectD tArea = transformRectBBox(tPixelToBoxel, tPixels);
        // Resmin gorunumu: ayni zoom, genis viewport; merkez, icerik pencereyle
        // birebir ayni yerde cizilecek sekilde kaydirilir
        XMapView tImageView = pView;
        tImageView.setViewport(tArea);
        const double tWorld = pView.worldSize();
        const double tShiftX = ((tArea.mLeft + tArea.mRight) - (tWindow.mLeft + tWindow.mRight)) * 0.5 / tWorld;
        const double tShiftY = ((tArea.mBottom + tArea.mTop) - (tWindow.mBottom + tWindow.mTop)) * 0.5 / tWorld;
        tImageView.setCenterMercator(pView.centerMercatorX() + tShiftX, pView.centerMercatorY() + tShiftY);

        if (!pCanvas.beginOffscreen(mTarget, tArea))
        {
            mValid = false;
            return false;
        }
        pRender(tImageView, tArea);
        pCanvas.endOffscreen();

        mValid = true;
        mPendingDirty = false;
        mLastRender = pNow;
        mCenterX = tImageView.centerMercatorX();
        mCenterY = tImageView.centerMercatorY();
        mWorldSize = tWorld;
        mArea = tArea;
        mWindowArea = tWindow;
        ++mCount;
    }
    blit(pCanvas, pView, pBoxelToPixel);
    return true;
}

void XMapImageCache::blit(Gfx::Canvas& pCanvas, const XMapView& pView, const Affine2D& pBoxelToPixel) const
{
    const RectD tRect = screenRectOfImage(pView, pBoxelToPixel);
    const float tLeft = static_cast<float>(tRect.mLeft);
    const float tRight = static_cast<float>(tRect.mRight);
    const float tBottom = static_cast<float>(tRect.mBottom);
    const float tTop = static_cast<float>(tRect.mTop);
    // FBO dokusunda satir 0 = alt kenar (v = 0 alt)
    const std::vector<Gfx::TexturedVertex> tQuad = {{tLeft, tTop, 0.0f, 1.0f},    {tLeft, tBottom, 0.0f, 0.0f},
                                                    {tRight, tBottom, 1.0f, 0.0f}, {tLeft, tTop, 0.0f, 1.0f},
                                                    {tRight, tBottom, 1.0f, 0.0f}, {tRight, tTop, 1.0f, 1.0f}};
    // Saydamliksiz kopya: dokunun alfasi yari saydam cizimlerde 1'in altina inebilir, ama renkler
    // zaten opak zemin uzerinde karistirilmis durumda
    pCanvas.drawTextured(mTarget.getTexture(), tQuad, false);
}
