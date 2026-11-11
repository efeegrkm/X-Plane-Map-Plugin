// MapImageCache.cpp
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

bool SameRect(const RectD& pFirst, const RectD& pSecond)
{
    const double tEpsilon = 1e-6;
    return std::abs(pFirst.mLeft - pSecond.mLeft) < tEpsilon && std::abs(pFirst.mRight - pSecond.mRight) < tEpsilon &&
           std::abs(pFirst.mBottom - pSecond.mBottom) < tEpsilon && std::abs(pFirst.mTop - pSecond.mTop) < tEpsilon;
}

// Mercator x farki: 180. boylam sarilmasiyla en kisa yon
double WrapDelta(double pDelta)
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

void MapImageCache::SetMaxRate(double pRendersPerSecond)
{
    mMaxRate = std::clamp(pRendersPerSecond, 1.0, 120.0);
}

void MapImageCache::Invalidate()
{
    mValid = false;
}

int MapImageCache::RendersPerSecond() const
{
    return mLastCount;
}

RectD MapImageCache::ScreenRectOfImage(const MapView& pView, const Affine2D& pBoxelToPixel) const
{
    // Resimdeki bir nokta p (boxel) -> ekran: M + (c0 - c) * W + (p - M0) * W / W0
    const RectD& tViewport = pView.Viewport();
    const double tWorld = pView.WorldSize();
    const double tScale = tWorld / mWorldSize;
    const double tMidX = (tViewport.mLeft + tViewport.mRight) * 0.5;
    const double tMidY = (tViewport.mBottom + tViewport.mTop) * 0.5;
    const double tOldMidX = (mArea.mLeft + mArea.mRight) * 0.5;
    const double tOldMidY = (mArea.mBottom + mArea.mTop) * 0.5;
    double tShiftX = tMidX + WrapDelta(mCenterX - pView.CenterMercatorX()) * tWorld - tOldMidX * tScale;
    double tShiftY = tMidY + (mCenterY - pView.CenterMercatorY()) * tWorld - tOldMidY * tScale;
    if (std::abs(tScale - 1.0) < 1e-9)
    {
        // Sadece kaydirma: tam piksele yuvarla (doku pikselleri ekran pikseline oturur, bulaniklik yok)
        const double tPixelsPerBoxel = PixelsPerBoxel(pBoxelToPixel);
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

bool MapImageCache::Draw(Gfx::Canvas& pCanvas, const MapView& pView, const Affine2D& pBoxelToPixel, float pNow,
                         bool pUrgent, bool pDirty, bool pBusy, const RenderFunction& pRender)
{
    // Teshis sayaci
    if (pNow - mCountStart >= 1.0f)
    {
        mLastCount = mCount;
        mCount = 0;
        mCountStart = pNow;
    }

    // Pencerenin piksel dikdortgeni ve payli doku boyu (tam piksel)
    const RectD& tWindow = pView.Viewport();
    const RectD tWindowPixels = TransformRectBBox(pBoxelToPixel, tWindow);
    const int tMarginX = static_cast<int>(std::lround(tWindowPixels.Width() * kMarginFraction));
    const int tMarginY = static_cast<int>(std::lround(tWindowPixels.Height() * kMarginFraction));
    RectD tPixels;
    tPixels.mLeft = std::floor(tWindowPixels.mLeft) - tMarginX;
    tPixels.mRight = std::ceil(tWindowPixels.mRight) + tMarginX;
    tPixels.mBottom = std::floor(tWindowPixels.mBottom) - tMarginY;
    tPixels.mTop = std::ceil(tWindowPixels.mTop) + tMarginY;
    const int tWidth = static_cast<int>(tPixels.Width());
    const int tHeight = static_cast<int>(tPixels.Height());
    Affine2D tPixelToBoxel;
    if (tWidth <= 0 || tHeight <= 0 || tWidth > kMaxTextureSide || tHeight > kMaxTextureSide ||
        !Invert(pBoxelToPixel, &tPixelToBoxel))
    {
        return false;
    }

    // Eski resim gecerli mi, pencereyi kapliyor mu?
    const bool tSizeChanged =
        tWidth != mTarget.Width() || tHeight != mTarget.Height() || !SameRect(tWindow, mWindowArea);
    bool tCovers = false;
    bool tViewChanged = true;
    if (mValid && !tSizeChanged)
    {
        const RectD tImage = ScreenRectOfImage(pView, pBoxelToPixel);
        const double tEpsilon = 0.01;
        tCovers = tImage.mLeft <= tWindow.mLeft + tEpsilon && tImage.mRight >= tWindow.mRight - tEpsilon &&
                  tImage.mBottom <= tWindow.mBottom + tEpsilon && tImage.mTop >= tWindow.mTop - tEpsilon;
        tViewChanged = !SameRect(tImage, mArea);
    }
    mPendingDirty = mPendingDirty || pDirty;

    const float tSinceLast = pNow - mLastRender;
    const bool tRateAllows = tSinceLast >= 1.0f / static_cast<float>(mMaxRate);
    const bool tRender = !mValid || tSizeChanged || pUrgent || !tCovers || tSinceLast >= kRefreshSeconds ||
                         (tRateAllows && (tViewChanged || mPendingDirty || pBusy));

    if (tRender)
    {
        if (mTarget.Prepare(tWidth, tHeight) == 0)
        {
            mValid = false;
            return false;
        }
        const RectD tArea = TransformRectBBox(tPixelToBoxel, tPixels);
        // Resmin gorunumu: ayni zoom, genis viewport; merkez, icerik pencereyle birebir ayni
        // yerde cizilecek sekilde kaydirilir (doku ortasi pencere ortasindan biraz farkli olabilir)
        MapView tImageView = pView;
        tImageView.SetViewport(tArea);
        const double tWorld = pView.WorldSize();
        const double tShiftX = ((tArea.mLeft + tArea.mRight) - (tWindow.mLeft + tWindow.mRight)) * 0.5 / tWorld;
        const double tShiftY = ((tArea.mBottom + tArea.mTop) - (tWindow.mBottom + tWindow.mTop)) * 0.5 / tWorld;
        tImageView.SetCenterMercator(pView.CenterMercatorX() + tShiftX, pView.CenterMercatorY() + tShiftY);

        if (!pCanvas.BeginOffscreen(mTarget, tArea))
        {
            mValid = false;
            return false;
        }
        pRender(tImageView, tArea);
        pCanvas.EndOffscreen();

        mValid = true;
        mPendingDirty = false;
        mLastRender = pNow;
        mCenterX = tImageView.CenterMercatorX();
        mCenterY = tImageView.CenterMercatorY();
        mWorldSize = tWorld;
        mArea = tArea;
        mWindowArea = tWindow;
        ++mCount;
    }
    Blit(pCanvas, pView, pBoxelToPixel);
    return true;
}

void MapImageCache::Blit(Gfx::Canvas& pCanvas, const MapView& pView, const Affine2D& pBoxelToPixel) const
{
    const RectD tRect = ScreenRectOfImage(pView, pBoxelToPixel);
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
    pCanvas.DrawTextured(mTarget.GetTexture(), tQuad, false);
}
