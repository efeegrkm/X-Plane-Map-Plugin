// GlApi.h
// OpenGL 3.3 fonksiyonlarini kullanilabilir hale getirir.
//
// Neden gerekli: Windows'un opengl32.lib'i sadece OpenGL 1.1 fonksiyonlarini
// (glDrawArrays, glGetIntegerv, glTexImage2D...) verir. Vertex buffer ve
// shader fonksiyonlari ekran kartinin surucusunde durur; calisma aninda
// wglGetProcAddress ile adresleri alinir. Bu dosya o adresleri bir kez
// toplar ve Gl::BindBuffer(...) gibi okunur fonksiyonlarla sunar.
//
// Load() sadece OpenGL baglami etkinken (pencerenin draw callback'i icinde)
// cagrilabilir. Projede bu dosya sadece gfx/ icinden kullanilir.
#pragma once

#include <cstddef>
#include <string>

#include "gfx/GlInclude.h"

// Windows'un gl.h'inde olmayan (OpenGL 1.2+) turler ve sabitler
using GLchar = char;
using GLsizeiptr = std::ptrdiff_t;

namespace Gl
{

const GLenum kArrayBuffer = 0x8892;           // GL_ARRAY_BUFFER
const GLenum kStreamDraw = 0x88E0;            // GL_STREAM_DRAW
const GLenum kFragmentShader = 0x8B30;        // GL_FRAGMENT_SHADER
const GLenum kVertexShader = 0x8B31;          // GL_VERTEX_SHADER
const GLenum kCompileStatus = 0x8B81;         // GL_COMPILE_STATUS
const GLenum kLinkStatus = 0x8B82;            // GL_LINK_STATUS
const GLenum kInfoLogLength = 0x8B84;         // GL_INFO_LOG_LENGTH
const GLenum kShadingLanguageVersion = 0x8B8C;  // GL_SHADING_LANGUAGE_VERSION
const GLenum kCurrentProgram = 0x8B8D;        // GL_CURRENT_PROGRAM
const GLenum kArrayBufferBinding = 0x8894;    // GL_ARRAY_BUFFER_BINDING
const GLenum kVertexArrayBinding = 0x85B5;    // GL_VERTEX_ARRAY_BINDING
const GLenum kMajorVersion = 0x821B;          // GL_MAJOR_VERSION
const GLenum kMinorVersion = 0x821C;          // GL_MINOR_VERSION
const GLenum kClampToEdge = 0x812F;           // GL_CLAMP_TO_EDGE
const GLenum kLinearMipmapLinear = 0x2703;    // GL_LINEAR_MIPMAP_LINEAR (GL 1.1 basligi da tanimlar)
const GLenum kTextureMaxLevel = 0x813D;       // GL_TEXTURE_MAX_LEVEL
const GLenum kFramebuffer = 0x8D40;           // GL_FRAMEBUFFER
const GLenum kDrawFramebuffer = 0x8CA9;       // GL_DRAW_FRAMEBUFFER
const GLenum kReadFramebuffer = 0x8CA8;       // GL_READ_FRAMEBUFFER
const GLenum kDrawFramebufferBinding = 0x8CA6;  // GL_DRAW_FRAMEBUFFER_BINDING
const GLenum kReadFramebufferBinding = 0x8CAA;  // GL_READ_FRAMEBUFFER_BINDING
const GLenum kColorAttachment0 = 0x8CE0;      // GL_COLOR_ATTACHMENT0
const GLenum kFramebufferComplete = 0x8CD5;   // GL_FRAMEBUFFER_COMPLETE
const GLenum kMaxTextureSize = 0x0D33;        // GL_MAX_TEXTURE_SIZE

// Fonksiyon adreslerini surucuden alir. Eksik fonksiyon ya da OpenGL < 3.3
// ise false doner, nedeni pOutError'a yazilir. Ikinci cagri bir sey yapmaz.
bool Load(std::string& pOutError);
bool IsLoaded();

// "4.6.0 NVIDIA 566.36 / NVIDIA GeForce RTX ... / GLSL 4.60" (log icin)
std::string DescribeContext();

// --- OpenGL baglami (context) ---
// Pencere pop-out yapilinca X-Plane onu AYRI bir OpenGL baglaminda cizer.
// Baglamlar dokulari, buffer'lari ve shader'lari paylasir ama vertex array
// (VAO) nesnelerini PAYLASMAZ: VAO'lar baglam basina olusturulmalidir.
void* CurrentContext();           // o an etkin baglamin kimligi (Windows: HGLRC); yoksa nullptr
void NoteContext();               // her pencere ciziminin basinda: baglam degistiyse nesli artirir
unsigned int ContextGeneration(); // baglam her degistiginde bir artar (VAO onbellekleri bunu izler)
GLboolean IsVertexArray(GLuint pArray);  // bu ad SU ANKI baglamda bir VAO mu
GLboolean IsProgram(GLuint pProgram);    // bu ad su anki baglamda bir shader programi mi

// --- Doku ---
void GenerateMipmap(GLenum pTarget);     // bagli dokunun kucultulmus seviyelerini surucu uretir (GL 3.0)

// --- Framebuffer (FBO, GL 3.0): baglamlar arasi PAYLASILMAZ (VAO gibi) ---
void GenFramebuffers(GLsizei pCount, GLuint* pFramebuffers);
void DeleteFramebuffers(GLsizei pCount, const GLuint* pFramebuffers);
void BindFramebuffer(GLenum pTarget, GLuint pFramebuffer);
void FramebufferTexture2D(GLenum pTarget, GLenum pAttachment, GLenum pTextureTarget, GLuint pTexture, GLint pLevel);
GLenum CheckFramebufferStatus(GLenum pTarget);

// --- Buffer / vertex array ---
void GenBuffers(GLsizei pCount, GLuint* pBuffers);
void DeleteBuffers(GLsizei pCount, const GLuint* pBuffers);
void BindBuffer(GLenum pTarget, GLuint pBuffer);
void BufferData(GLenum pTarget, GLsizeiptr pSize, const void* pData, GLenum pUsage);
void GenVertexArrays(GLsizei pCount, GLuint* pArrays);
void DeleteVertexArrays(GLsizei pCount, const GLuint* pArrays);
void BindVertexArray(GLuint pArray);
void EnableVertexAttribArray(GLuint pIndex);
void VertexAttribPointer(GLuint pIndex, GLint pSize, GLenum pType, GLboolean pNormalized, GLsizei pStride,
                         std::size_t pOffset);

// --- Shader / program ---
GLuint CreateShader(GLenum pType);
void DeleteShader(GLuint pShader);
void ShaderSource(GLuint pShader, const char* pSource);
void CompileShader(GLuint pShader);
GLint GetShaderInt(GLuint pShader, GLenum pName);
std::string GetShaderLog(GLuint pShader);
GLuint CreateProgram();
void DeleteProgram(GLuint pProgram);
void AttachShader(GLuint pProgram, GLuint pShader);
void LinkProgram(GLuint pProgram);
GLint GetProgramInt(GLuint pProgram, GLenum pName);
std::string GetProgramLog(GLuint pProgram);
void UseProgram(GLuint pProgram);
GLint GetUniformLocation(GLuint pProgram, const char* pName);
void UniformMatrix4(GLint pLocation, const float pMatrix[16]);
void Uniform1i(GLint pLocation, GLint pValue);
void Uniform1f(GLint pLocation, float pValue);
void Uniform4(GLint pLocation, float pX, float pY, float pZ, float pW);

}  // namespace Gl
