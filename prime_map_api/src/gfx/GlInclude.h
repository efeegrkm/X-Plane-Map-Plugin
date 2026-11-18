// OpenGL basliklarini platforma uygun sekilde dahil eder.
#if IBM
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#if APL
#include <openGL/gl.h>
#else
#include <GL/gl.h>
#endif
