// openGL 3.3 yukleyici: Windows opengl32.lib sadece GL 1.1 fonksiyonlarini
// verir; gerisi wglGetProcAddress ile alinir. load() yalnizca GL baglami
// etkinken (draw callback) cagrilabilir.
#ifndef PRIME_MAP_SRC_GFX_GLAPI_H
#define PRIME_MAP_SRC_GFX_GLAPI_H

#include <cstddef>
#include <string>

#include "gfx/GlInclude.h"

// Windows'un gl.h'inde olmayan (openGL 1.2+) turler ve sabitler
using GLchar = char;
using GLsizeiptr = std::ptrdiff_t;

namespace Gl
{

const GLenum kArrayBuffer = 0x8892;
const GLenum kStreamDraw = 0x88E0;
const GLenum kFragmentShader = 0x8B30;
const GLenum kVertexShader = 0x8B31;
const GLenum kCompileStatus = 0x8B81;
const GLenum kLinkStatus = 0x8B82;
const GLenum kInfoLogLength = 0x8B84;
const GLenum kShadingLanguageVersion = 0x8B8C;
const GLenum kCurrentProgram = 0x8B8D;
const GLenum kArrayBufferBinding = 0x8894;
const GLenum kVertexArrayBinding = 0x85B5;
const GLenum kMajorVersion = 0x821B;
const GLenum kMinorVersion = 0x821C;
const GLenum kClampToEdge = 0x812F;
const GLenum kLinearMipmapLinear = 0x2703;  // GL 1.1 basligi da tanimlar
const GLenum kTextureMaxLevel = 0x813D;
const GLenum kFramebuffer = 0x8D40;
const GLenum kDrawFramebuffer = 0x8CA9;
const GLenum kReadFramebuffer = 0x8CA8;
const GLenum kDrawFramebufferBinding = 0x8CA6;
const GLenum kReadFramebufferBinding = 0x8CAA;
const GLenum kColorAttachment0 = 0x8CE0;
const GLenum kFramebufferComplete = 0x8CD5;
const GLenum kMaxTextureSize = 0x0D33;

// Eksik fonksiyon ya da openGL < 3.3 ise false, nedeni pOutError'a yazilir.
bool load(std::string& pOutError);
bool isLoaded();

// ornek: "4.6.0 NVIDIA 566.36 / NVIDIA GeForce RTX ... / GLSL 4.60"
std::string describeContext();

// Pop-out pencere AYRI bir openGL baglaminda cizilir; baglamlar doku/buffer/
// shader paylasir ama VAO nesnelerini PAYLASMAZ (baglam basina olusturulmali).
void* currentContext();
// her pencere ciziminin basinda: baglam degistiyse nesli artirir
void noteContext();
// baglam her degistiginde bir artar (VAO onbellekleri bunu izler)
unsigned int contextGeneration();
GLboolean isVertexArray(GLuint pArray);
GLboolean isProgram(GLuint pProgram);

void generateMipmap(GLenum pTarget);

// framebuffer (FBO) da baglamlar arasi PAYLASILMAZ (VAO gibi)
void genFramebuffers(GLsizei pCount, GLuint* pFramebuffers);
void deleteFramebuffers(GLsizei pCount, const GLuint* pFramebuffers);
void bindFramebuffer(GLenum pTarget, GLuint pFramebuffer);
void framebufferTexture2D(GLenum pTarget, GLenum pAttachment, GLenum pTextureTarget, GLuint pTexture, GLint pLevel);
GLenum checkFramebufferStatus(GLenum pTarget);

void genBuffers(GLsizei pCount, GLuint* pBuffers);
void deleteBuffers(GLsizei pCount, const GLuint* pBuffers);
void bindBuffer(GLenum pTarget, GLuint pBuffer);
void bufferData(GLenum pTarget, GLsizeiptr pSize, const void* pData, GLenum pUsage);
void genVertexArrays(GLsizei pCount, GLuint* pArrays);
void deleteVertexArrays(GLsizei pCount, const GLuint* pArrays);
void bindVertexArray(GLuint pArray);
void enableVertexAttribArray(GLuint pIndex);
void vertexAttribPointer(GLuint pIndex, GLint pSize, GLenum pType, GLboolean pNormalized, GLsizei pStride,
                         std::size_t pOffset);

GLuint createShader(GLenum pType);
void deleteShader(GLuint pShader);
void shaderSource(GLuint pShader, const char* pSource);
void compileShader(GLuint pShader);
GLint getShaderInt(GLuint pShader, GLenum pName);
std::string getShaderLog(GLuint pShader);
GLuint createProgram();
void deleteProgram(GLuint pProgram);
void attachShader(GLuint pProgram, GLuint pShader);
void linkProgram(GLuint pProgram);
GLint getProgramInt(GLuint pProgram, GLenum pName);
std::string getProgramLog(GLuint pProgram);
void useProgram(GLuint pProgram);
GLint getUniformLocation(GLuint pProgram, const char* pName);
void uniformMatrix4(GLint pLocation, const float pMatrix[16]);
void uniform1i(GLint pLocation, GLint pValue);
void uniform1f(GLint pLocation, float pValue);
void uniform4(GLint pLocation, float pX, float pY, float pZ, float pW);

}
#endif  // PRIME_MAP_SRC_GFX_GLAPI_H
