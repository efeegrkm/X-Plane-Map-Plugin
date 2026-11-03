// GlScreen.cpp
#include "GlScreen.h"

#include <cmath>

bool ReadBoxelToPixel(Affine2D* pOut)
{
    float tModelview[16];
    float tProjection[16];
    GLint tViewport[4];
    glGetFloatv(GL_MODELVIEW_MATRIX, tModelview);
    glGetFloatv(GL_PROJECTION_MATRIX, tProjection);
    glGetIntegerv(GL_VIEWPORT, tViewport);
    const int tViewportInt[4] = {tViewport[0], tViewport[1], tViewport[2], tViewport[3]};
    return AffineFromGL(tModelview, tProjection, tViewportInt, pOut);
}

double PixelsPerBoxel(const Affine2D& pBoxelToPixel)
{
    return std::sqrt(std::fabs(pBoxelToPixel.mA * pBoxelToPixel.mD - pBoxelToPixel.mB * pBoxelToPixel.mC));
}

ScissorGuard::ScissorGuard(const Affine2D& pBoxelToPixel, const RectD& pBoxels)
    : mWasEnabled(glIsEnabled(GL_SCISSOR_TEST))
{
    glGetIntegerv(GL_SCISSOR_BOX, mOldBox);

    RectD tPixels = TransformRectBBox(pBoxelToPixel, pBoxels);
    if (mWasEnabled)
    {
        RectD tOld;
        tOld.mLeft = mOldBox[0];
        tOld.mBottom = mOldBox[1];
        tOld.mRight = mOldBox[0] + mOldBox[2];
        tOld.mTop = mOldBox[1] + mOldBox[3];
        RectD tBoth;
        if (IntersectRect(tPixels, tOld, &tBoth))
        {
            tPixels = tBoth;
        }
    }
    mPixelRect = tPixels;

    const GLint tX = static_cast<GLint>(std::floor(tPixels.mLeft + 0.5));
    const GLint tY = static_cast<GLint>(std::floor(tPixels.mBottom + 0.5));
    const GLint tRight = static_cast<GLint>(std::floor(tPixels.mRight + 0.5));
    const GLint tTop = static_cast<GLint>(std::floor(tPixels.mTop + 0.5));
    glEnable(GL_SCISSOR_TEST);
    glScissor(tX, tY, tRight > tX ? tRight - tX : 0, tTop > tY ? tTop - tY : 0);
}

ScissorGuard::~ScissorGuard()
{
    glScissor(mOldBox[0], mOldBox[1], mOldBox[2], mOldBox[3]);
    if (!mWasEnabled)
    {
        glDisable(GL_SCISSOR_TEST);
    }
}

const RectD& ScissorGuard::PixelRect() const
{
    return mPixelRect;
}
