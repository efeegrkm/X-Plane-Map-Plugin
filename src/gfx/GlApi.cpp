// GlApi.cpp
#include "gfx/GlApi.h"

#include <cstdint>
#include <vector>

namespace
{

// Surucudeki fonksiyonlarin imzalari (OpenGL belgesindekiyle ayni)
using GenBuffersFn = void(APIENTRY*)(GLsizei, GLuint*);
using DeleteBuffersFn = void(APIENTRY*)(GLsizei, const GLuint*);
using BindBufferFn = void(APIENTRY*)(GLenum, GLuint);
using BufferDataFn = void(APIENTRY*)(GLenum, GLsizeiptr, const void*, GLenum);
using GenVertexArraysFn = void(APIENTRY*)(GLsizei, GLuint*);
using DeleteVertexArraysFn = void(APIENTRY*)(GLsizei, const GLuint*);
using BindVertexArrayFn = void(APIENTRY*)(GLuint);
using EnableVertexAttribArrayFn = void(APIENTRY*)(GLuint);
using VertexAttribPointerFn = void(APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
using CreateShaderFn = GLuint(APIENTRY*)(GLenum);
using DeleteShaderFn = void(APIENTRY*)(GLuint);
using ShaderSourceFn = void(APIENTRY*)(GLuint, GLsizei, const GLchar* const*, const GLint*);
using CompileShaderFn = void(APIENTRY*)(GLuint);
using GetShaderivFn = void(APIENTRY*)(GLuint, GLenum, GLint*);
using GetShaderInfoLogFn = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*);
using CreateProgramFn = GLuint(APIENTRY*)();
using DeleteProgramFn = void(APIENTRY*)(GLuint);
using AttachShaderFn = void(APIENTRY*)(GLuint, GLuint);
using LinkProgramFn = void(APIENTRY*)(GLuint);
using GetProgramivFn = void(APIENTRY*)(GLuint, GLenum, GLint*);
using GetProgramInfoLogFn = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*);
using UseProgramFn = void(APIENTRY*)(GLuint);
using GetUniformLocationFn = GLint(APIENTRY*)(GLuint, const GLchar*);
using UniformMatrix4fvFn = void(APIENTRY*)(GLint, GLsizei, GLboolean, const float*);
using Uniform1iFn = void(APIENTRY*)(GLint, GLint);
using Uniform1fFn = void(APIENTRY*)(GLint, float);

// Yuklenen adresler. Surec boyunca tek bir OpenGL surucusu oldugu icin
// global tutulmalari dogal (X-Plane'in butun baglamlari ayni surucuden).
struct Functions
{
    GenBuffersFn mGenBuffers = nullptr;
    DeleteBuffersFn mDeleteBuffers = nullptr;
    BindBufferFn mBindBuffer = nullptr;
    BufferDataFn mBufferData = nullptr;
    GenVertexArraysFn mGenVertexArrays = nullptr;
    DeleteVertexArraysFn mDeleteVertexArrays = nullptr;
    BindVertexArrayFn mBindVertexArray = nullptr;
    EnableVertexAttribArrayFn mEnableVertexAttribArray = nullptr;
    VertexAttribPointerFn mVertexAttribPointer = nullptr;
    CreateShaderFn mCreateShader = nullptr;
    DeleteShaderFn mDeleteShader = nullptr;
    ShaderSourceFn mShaderSource = nullptr;
    CompileShaderFn mCompileShader = nullptr;
    GetShaderivFn mGetShaderiv = nullptr;
    GetShaderInfoLogFn mGetShaderInfoLog = nullptr;
    CreateProgramFn mCreateProgram = nullptr;
    DeleteProgramFn mDeleteProgram = nullptr;
    AttachShaderFn mAttachShader = nullptr;
    LinkProgramFn mLinkProgram = nullptr;
    GetProgramivFn mGetProgramiv = nullptr;
    GetProgramInfoLogFn mGetProgramInfoLog = nullptr;
    UseProgramFn mUseProgram = nullptr;
    GetUniformLocationFn mGetUniformLocation = nullptr;
    UniformMatrix4fvFn mUniformMatrix4fv = nullptr;
    Uniform1iFn mUniform1i = nullptr;
    Uniform1fFn mUniform1f = nullptr;
};

Functions gFunctions;
bool gLoaded = false;

// Bir fonksiyonun adresini surucuden al. wglGetProcAddress hata durumunda
// nullptr disinda 1, 2, 3 veya -1 de donebilir; hepsi "yok" demek.
template <typename TFunction>
bool LoadOne(const char* pName, TFunction& pOut, std::string& pMissing)
{
#if IBM
    const PROC tAddress = wglGetProcAddress(pName);
    const std::intptr_t tValue = reinterpret_cast<std::intptr_t>(tAddress);
    if (tValue == 0 || tValue == 1 || tValue == 2 || tValue == 3 || tValue == -1)
    {
        pMissing += pMissing.empty() ? pName : std::string(", ") + pName;
        return false;
    }
    pOut = reinterpret_cast<TFunction>(tAddress);
    return true;
#else
    (void)pOut;
    pMissing += pMissing.empty() ? pName : std::string(", ") + pName;
    return false;
#endif
}

std::string GlString(GLenum pName)
{
    const GLubyte* tText = glGetString(pName);
    return tText ? reinterpret_cast<const char*>(tText) : "?";
}

}  // namespace

namespace Gl
{

bool Load(std::string& pOutError)
{
    if (gLoaded)
    {
        return true;
    }

    // Surum: OpenGL 3.3 ve ustu gerekli (shader dili 3.30, vertex array)
    GLint tMajor = 0;
    GLint tMinor = 0;
    glGetIntegerv(kMajorVersion, &tMajor);
    glGetIntegerv(kMinorVersion, &tMinor);
    if (tMajor < 3 || (tMajor == 3 && tMinor < 3))
    {
        pOutError = "OpenGL 3.3 required, context is " + GlString(GL_VERSION);
        return false;
    }

    std::string tMissing;
    Functions& tF = gFunctions;
    LoadOne("glGenBuffers", tF.mGenBuffers, tMissing);
    LoadOne("glDeleteBuffers", tF.mDeleteBuffers, tMissing);
    LoadOne("glBindBuffer", tF.mBindBuffer, tMissing);
    LoadOne("glBufferData", tF.mBufferData, tMissing);
    LoadOne("glGenVertexArrays", tF.mGenVertexArrays, tMissing);
    LoadOne("glDeleteVertexArrays", tF.mDeleteVertexArrays, tMissing);
    LoadOne("glBindVertexArray", tF.mBindVertexArray, tMissing);
    LoadOne("glEnableVertexAttribArray", tF.mEnableVertexAttribArray, tMissing);
    LoadOne("glVertexAttribPointer", tF.mVertexAttribPointer, tMissing);
    LoadOne("glCreateShader", tF.mCreateShader, tMissing);
    LoadOne("glDeleteShader", tF.mDeleteShader, tMissing);
    LoadOne("glShaderSource", tF.mShaderSource, tMissing);
    LoadOne("glCompileShader", tF.mCompileShader, tMissing);
    LoadOne("glGetShaderiv", tF.mGetShaderiv, tMissing);
    LoadOne("glGetShaderInfoLog", tF.mGetShaderInfoLog, tMissing);
    LoadOne("glCreateProgram", tF.mCreateProgram, tMissing);
    LoadOne("glDeleteProgram", tF.mDeleteProgram, tMissing);
    LoadOne("glAttachShader", tF.mAttachShader, tMissing);
    LoadOne("glLinkProgram", tF.mLinkProgram, tMissing);
    LoadOne("glGetProgramiv", tF.mGetProgramiv, tMissing);
    LoadOne("glGetProgramInfoLog", tF.mGetProgramInfoLog, tMissing);
    LoadOne("glUseProgram", tF.mUseProgram, tMissing);
    LoadOne("glGetUniformLocation", tF.mGetUniformLocation, tMissing);
    LoadOne("glUniformMatrix4fv", tF.mUniformMatrix4fv, tMissing);
    LoadOne("glUniform1i", tF.mUniform1i, tMissing);
    LoadOne("glUniform1f", tF.mUniform1f, tMissing);
    if (!tMissing.empty())
    {
        pOutError = "OpenGL functions missing: " + tMissing;
        return false;
    }
    gLoaded = true;
    return true;
}

bool IsLoaded()
{
    return gLoaded;
}

std::string DescribeContext()
{
    return GlString(GL_VERSION) + " / " + GlString(GL_RENDERER) + " / GLSL " + GlString(kShadingLanguageVersion);
}

void GenBuffers(GLsizei pCount, GLuint* pBuffers)
{
    gFunctions.mGenBuffers(pCount, pBuffers);
}

void DeleteBuffers(GLsizei pCount, const GLuint* pBuffers)
{
    gFunctions.mDeleteBuffers(pCount, pBuffers);
}

void BindBuffer(GLenum pTarget, GLuint pBuffer)
{
    gFunctions.mBindBuffer(pTarget, pBuffer);
}

void BufferData(GLenum pTarget, GLsizeiptr pSize, const void* pData, GLenum pUsage)
{
    gFunctions.mBufferData(pTarget, pSize, pData, pUsage);
}

void GenVertexArrays(GLsizei pCount, GLuint* pArrays)
{
    gFunctions.mGenVertexArrays(pCount, pArrays);
}

void DeleteVertexArrays(GLsizei pCount, const GLuint* pArrays)
{
    gFunctions.mDeleteVertexArrays(pCount, pArrays);
}

void BindVertexArray(GLuint pArray)
{
    gFunctions.mBindVertexArray(pArray);
}

void EnableVertexAttribArray(GLuint pIndex)
{
    gFunctions.mEnableVertexAttribArray(pIndex);
}

void VertexAttribPointer(GLuint pIndex, GLint pSize, GLenum pType, GLboolean pNormalized, GLsizei pStride,
                         std::size_t pOffset)
{
    // OpenGL bu parametreyi isaretci olarak ister ama anlami "buffer icindeki bayt konumu"
    gFunctions.mVertexAttribPointer(pIndex, pSize, pType, pNormalized, pStride,
                                    reinterpret_cast<const void*>(pOffset));
}

GLuint CreateShader(GLenum pType)
{
    return gFunctions.mCreateShader(pType);
}

void DeleteShader(GLuint pShader)
{
    gFunctions.mDeleteShader(pShader);
}

void ShaderSource(GLuint pShader, const char* pSource)
{
    gFunctions.mShaderSource(pShader, 1, &pSource, nullptr);
}

void CompileShader(GLuint pShader)
{
    gFunctions.mCompileShader(pShader);
}

GLint GetShaderInt(GLuint pShader, GLenum pName)
{
    GLint tValue = 0;
    gFunctions.mGetShaderiv(pShader, pName, &tValue);
    return tValue;
}

std::string GetShaderLog(GLuint pShader)
{
    std::vector<GLchar> tLog(static_cast<size_t>(GetShaderInt(pShader, kInfoLogLength)) + 1, '\0');
    gFunctions.mGetShaderInfoLog(pShader, static_cast<GLsizei>(tLog.size()), nullptr, tLog.data());
    return tLog.data();
}

GLuint CreateProgram()
{
    return gFunctions.mCreateProgram();
}

void DeleteProgram(GLuint pProgram)
{
    gFunctions.mDeleteProgram(pProgram);
}

void AttachShader(GLuint pProgram, GLuint pShader)
{
    gFunctions.mAttachShader(pProgram, pShader);
}

void LinkProgram(GLuint pProgram)
{
    gFunctions.mLinkProgram(pProgram);
}

GLint GetProgramInt(GLuint pProgram, GLenum pName)
{
    GLint tValue = 0;
    gFunctions.mGetProgramiv(pProgram, pName, &tValue);
    return tValue;
}

std::string GetProgramLog(GLuint pProgram)
{
    std::vector<GLchar> tLog(static_cast<size_t>(GetProgramInt(pProgram, kInfoLogLength)) + 1, '\0');
    gFunctions.mGetProgramInfoLog(pProgram, static_cast<GLsizei>(tLog.size()), nullptr, tLog.data());
    return tLog.data();
}

void UseProgram(GLuint pProgram)
{
    gFunctions.mUseProgram(pProgram);
}

GLint GetUniformLocation(GLuint pProgram, const char* pName)
{
    return gFunctions.mGetUniformLocation(pProgram, pName);
}

void UniformMatrix4(GLint pLocation, const float pMatrix[16])
{
    gFunctions.mUniformMatrix4fv(pLocation, 1, GL_FALSE, pMatrix);
}

void Uniform1i(GLint pLocation, GLint pValue)
{
    gFunctions.mUniform1i(pLocation, pValue);
}

void Uniform1f(GLint pLocation, float pValue)
{
    gFunctions.mUniform1f(pLocation, pValue);
}

}  // namespace Gl
