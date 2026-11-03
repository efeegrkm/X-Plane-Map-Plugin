// ScreenState.cpp
#include "ScreenState.h"

#include "GLInclude.h"
#include "Log.h"

namespace Screen {

namespace {
bool g_has = false;
Affine2D g_boxelToPixel;
Affine2D g_pixelToBoxel;
int g_framebuffer = -1;
bool g_warned = false;
}  // namespace

void CaptureWindowTransform(bool readFramebuffer) {
    float mv[16], pr[16];
    GLint vp[4];
    glGetFloatv(GL_MODELVIEW_MATRIX, mv);
    glGetFloatv(GL_PROJECTION_MATRIX, pr);
    glGetIntegerv(GL_VIEWPORT, vp);
    int viewport[4] = {vp[0], vp[1], vp[2], vp[3]};

    Affine2D m, inv;
    if (!AffineFromGL(mv, pr, viewport, &m) || !Invert(m, &inv)) {
        if (!g_warned) {
            Log("UYARI: pencere matrisi okunamadi (2B degil)");
            g_warned = true;
        }
        return;
    }
    g_boxelToPixel = m;
    g_pixelToBoxel = inv;
    g_has = true;

    if (readFramebuffer) {
        GLint fb = -1;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fb);
        while (glGetError() != GL_NO_ERROR) {}  // desteklenmiyorsa olusan hatayi temizle
        g_framebuffer = fb;
    }
}

bool HasWindowTransform() { return g_has; }

bool BoxelToPixel(double bx, double by, double* px, double* py) {
    if (!g_has) return false;
    g_boxelToPixel.Apply(bx, by, px, py);
    return true;
}

bool PixelRectToBoxel(const RectD& pixels, RectD* boxels) {
    if (!g_has) return false;
    *boxels = TransformRectBBox(g_pixelToBoxel, pixels);
    return true;
}

const Affine2D& BoxelToPixelAffine() { return g_boxelToPixel; }

int WindowFramebuffer() { return g_framebuffer; }

}  // namespace Screen
