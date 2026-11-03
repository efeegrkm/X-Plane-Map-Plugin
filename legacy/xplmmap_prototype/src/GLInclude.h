// GLInclude.h
// OpenGL basliklarini her platformda dogru sekilde dahil eder.
// X-Plane 11 eski (uyumluluk modu) OpenGL kullanir; glBegin/glEnd calisir.
#pragma once

#if IBM
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>  // Windows'ta gl.h'den once gelmeli
#endif

#if APL
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

// OpenGL 1.1 basliginda olmayan sabitler (sadece teshis icin okunur)
#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif

// X-Plane bazi durumlarda "yuz ayiklama"yi (GL_CULL_FACE) acik birakiyor.
// O zaman ters yonde cizilen dolu sekiller (kare, ucgen, daire) hic
// gorunmuyor; cizgiler etkilenmiyor. Cizim boyunca kapatip sonra eski
// haline donduruyoruz:  { CullFaceOff guard;  ...cizim... }
class CullFaceOff {
public:
    CullFaceOff() : wasOn_(glIsEnabled(GL_CULL_FACE)) {
        if (wasOn_) glDisable(GL_CULL_FACE);
    }
    ~CullFaceOff() {
        if (wasOn_) glEnable(GL_CULL_FACE);
    }

private:
    GLboolean wasOn_;
};
