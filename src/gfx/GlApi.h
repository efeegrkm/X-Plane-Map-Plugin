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

// Fonksiyon adreslerini surucuden alir. Eksik fonksiyon ya da OpenGL < 3.3
// ise false doner, nedeni pOutError'a yazilir. Ikinci cagri bir sey yapmaz.
bool Load(std::string& pOutError);
bool IsLoaded();

// "4.6.0 NVIDIA 566.36 / NVIDIA GeForce RTX ... / GLSL 4.60" (log icin)
std::string DescribeContext();

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

}  // namespace Gl
