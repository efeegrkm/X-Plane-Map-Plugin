// Texture.cpp
#include "gfx/Texture.h"

#include "XPLMGraphics.h"

#include "GLInclude.h"
#include "MapView.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F  // OpenGL 1.2; Windows'un gl.h'i 1.1
#endif

namespace Gfx
{

int CreateTexture(int pSize, const uint8_t* pRgba)
{
    int tTexture = 0;
    XPLMGenerateTextureNumbers(&tTexture, 1);
    XPLMBindTexture2d(tTexture, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pSize, pSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, pRgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  // komsu karoyla dikis olmasin
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tTexture;
}

void DeleteTexture(int pTexture)
{
    const GLuint tTexture = static_cast<GLuint>(pTexture);
    glDeleteTextures(1, &tTexture);
}

void DrawTextureLatLon(int pTexture, const MapView& pView, double pWest, double pSouth, double pEast, double pNorth,
                       int pStrips)
{
    XPLMSetGraphicsState(0, 1, 0, 0, 1, 0, 0);  // 1 doku birimi, saydamlik acik
    XPLMBindTexture2d(pTexture, 0);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUAD_STRIP);
    for (int tStrip = 0; tStrip <= pStrips; ++tStrip)
    {
        // t: doku satiri (0 = kuzey); enlem dogrusal, ekrandaki y Mercator
        const double tT = static_cast<double>(tStrip) / pStrips;
        const double tLat = pNorth - (pNorth - pSouth) * tT;
        double tLeftX = 0.0;
        double tY = 0.0;
        double tRightX = 0.0;
        double tUnused = 0.0;
        pView.LatLonToBoxel({tLat, pWest}, &tLeftX, &tY);
        pView.LatLonToBoxel({tLat, pEast}, &tRightX, &tUnused);
        glTexCoord2f(0.0f, static_cast<float>(tT));
        glVertex2f(static_cast<float>(tLeftX), static_cast<float>(tY));
        glTexCoord2f(1.0f, static_cast<float>(tT));
        glVertex2f(static_cast<float>(tRightX), static_cast<float>(tY));
    }
    glEnd();
}

}  // namespace Gfx
