// GlInclude.h
// OpenGL basliklarini her platformda dogru sekilde dahil eder.
// Sadece gfx/ klasorundeki dosyalar (ve onizleme araci) kullanir.
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
