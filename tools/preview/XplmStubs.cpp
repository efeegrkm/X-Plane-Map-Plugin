// XplmStubs.cpp
// Onizleme araci icin X-Plane SDK fonksiyonlarinin basit karsiliklari.
// Eklentinin cizim kodu (katmanlar, arayuz) bunlari cagirir; X-Plane yokken
// ayni kodu bir OpenGL penceresinde calistirip goruntuyu dosyaya yazabiliriz.
//
// Sadece cizim kodunun kullandiklari var. Yazi, Windows'un yazi tipiyle
// (wglUseFontBitmaps) cizilir; X-Plane'in yazi tipine yakin boyutta.
#include <cstdio>
#include <cstring>

#include "XPLMDataAccess.h"
#include "XPLMGraphics.h"
#include "XPLMNavigation.h"
#include "XPLMUtilities.h"

#include "GLInclude.h"
#include "tools/preview/XplmStubs.h"

namespace
{

HDC gFontDc = nullptr;
GLuint gFontListBase = 0;
const int kFontHeight = 10;  // X-Plane'in xplmFont_Proportional'ina yakin

}  // namespace

void PreviewInitFont(HDC pDc)
{
    gFontDc = pDc;
    HFONT tFont = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_PRECIS,
                              CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, VARIABLE_PITCH, "Arial");
    SelectObject(pDc, tFont);
    gFontListBase = glGenLists(256);
    wglUseFontBitmapsA(pDc, 0, 256, gFontListBase);
}

namespace
{

// Bir OpenGL ozelligini ac/kapat
void SetCapability(GLenum pCapability, bool pOn)
{
    if (pOn)
    {
        glEnable(pCapability);
    }
    else
    {
        glDisable(pCapability);
    }
}

}  // namespace

XPLM_API void XPLMSetGraphicsState(int inEnableFog, int inNumberTexUnits, int inEnableLighting,
                                   int inEnableAlphaTesting, int inEnableAlphaBlending, int inEnableDepthTesting,
                                   int inEnableDepthWriting)
{
    SetCapability(GL_FOG, inEnableFog != 0);
    SetCapability(GL_TEXTURE_2D, inNumberTexUnits > 0);
    SetCapability(GL_LIGHTING, inEnableLighting != 0);
    SetCapability(GL_ALPHA_TEST, inEnableAlphaTesting != 0);
    SetCapability(GL_BLEND, inEnableAlphaBlending != 0);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    SetCapability(GL_DEPTH_TEST, inEnableDepthTesting != 0);
    glDepthMask(inEnableDepthWriting ? GL_TRUE : GL_FALSE);
}

XPLM_API void XPLMBindTexture2d(int inTextureNum, int /*inTextureUnit*/)
{
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(inTextureNum));
}

XPLM_API void XPLMGenerateTextureNumbers(int* outTextureIDs, int inCount)
{
    for (int tIndex = 0; tIndex < inCount; ++tIndex)
    {
        GLuint tTexture = 0;
        glGenTextures(1, &tTexture);
        outTextureIDs[tIndex] = static_cast<int>(tTexture);
    }
}

XPLM_API void XPLMDrawString(float* inColorRGB, int inXOffset, int inYOffset, const char* inChar,
                             int* /*inWordWrapWidth*/, XPLMFontID /*inFontID*/)
{
    glDisable(GL_TEXTURE_2D);
    glColor3f(inColorRGB[0], inColorRGB[1], inColorRGB[2]);
    glRasterPos2i(inXOffset, inYOffset);
    glListBase(gFontListBase);
    glCallLists(static_cast<GLsizei>(std::strlen(inChar)), GL_UNSIGNED_BYTE, inChar);
}

XPLM_API float XPLMMeasureString(XPLMFontID /*inFontID*/, const char* inChar, int inNumChars)
{
    SIZE tSize = {0, 0};
    GetTextExtentPoint32A(gFontDc, inChar, inNumChars, &tSize);
    return static_cast<float>(tSize.cx);
}

XPLM_API void XPLMGetFontDimensions(XPLMFontID /*inFontID*/, int* outCharWidth, int* outCharHeight,
                                    int* outDigitsOnly)
{
    if (outCharWidth)
    {
        *outCharWidth = 7;
    }
    if (outCharHeight)
    {
        *outCharHeight = kFontHeight;
    }
    if (outDigitsOnly)
    {
        *outDigitsOnly = 0;
    }
}

XPLM_API void XPLMDebugString(const char* inString)
{
    std::fputs(inString, stdout);
}

// --- Ucak ve FMS: onizlemede yok ---

XPLM_API XPLMDataRef XPLMFindDataRef(const char* /*inDataRefName*/)
{
    return nullptr;
}

XPLM_API double XPLMGetDatad(XPLMDataRef /*inDataRef*/)
{
    return 0.0;
}

XPLM_API float XPLMGetDataf(XPLMDataRef /*inDataRef*/)
{
    return 0.0f;
}

XPLM_API int XPLMCountFMSEntries(void)
{
    return 0;
}

XPLM_API int XPLMGetDestinationFMSEntry(void)
{
    return -1;
}

XPLM_API void XPLMGetFMSEntryInfo(int /*inIndex*/, XPLMNavType* /*outType*/, char* /*outID*/, XPLMNavRef* /*outRef*/,
                                  int* /*outAltitude*/, float* /*outLat*/, float* /*outLon*/)
{
}

XPLM_API void XPLMGetNavAidInfo(XPLMNavRef /*inRef*/, XPLMNavType* /*outType*/, float* /*outLatitude*/,
                                float* /*outLongitude*/, float* /*outHeight*/, int* /*outFrequency*/,
                                float* /*outHeading*/, char* /*outID*/, char* /*outName*/, char* /*outReg*/)
{
}
