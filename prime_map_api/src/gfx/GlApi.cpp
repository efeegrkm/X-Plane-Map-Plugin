#include "gfx/GlApi.h"

#include <cstdint>
#include <vector>

namespace
{

using GenBuffersFn = void(APIENTRY*)(GLsizei, GLuint*);
using DeleteBuffersFn = void(APIENTRY*)(GLsizei, const GLuint*);
using BindBufferFn = void(APIENTRY*)(GLenum, GLuint);
using BufferDataFn = void(APIENTRY*)(GLenum, GLsizeiptr, const void*, GLenum);
using GenVertexArraysFn = void(APIENTRY*)(GLsizei, GLuint*);
using DeleteVertexArraysFn = void(APIENTRY*)(GLsizei, const GLuint*);
using BindVertexArrayFn = void(APIENTRY*)(GLuint);
using EnableVertexAttribArrayFn = void(APIENTRY*)(GLuint);
using VertexAttribPointerFn = void(APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
using createShaderFn = GLuint(APIENTRY*)(GLenum);
using DeleteShaderFn = void(APIENTRY*)(GLuint);
using ShaderSourceFn = void(APIENTRY*)(GLuint, GLsizei, const GLchar* const*, const GLint*);
using CompileShaderFn = void(APIENTRY*)(GLuint);
using getShaderivFn = void(APIENTRY*)(GLuint, GLenum, GLint*);
using getShaderInfoLogFn = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*);
using createProgramFn = GLuint(APIENTRY*)();
using DeleteProgramFn = void(APIENTRY*)(GLuint);
using AttachShaderFn = void(APIENTRY*)(GLuint, GLuint);
using LinkProgramFn = void(APIENTRY*)(GLuint);
using getProgramivFn = void(APIENTRY*)(GLuint, GLenum, GLint*);
using getProgramInfoLogFn = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*);
using UseProgramFn = void(APIENTRY*)(GLuint);
using getUniformLocationFn = GLint(APIENTRY*)(GLuint, const GLchar*);
using UniformMatrix4fvFn = void(APIENTRY*)(GLint, GLsizei, GLboolean, const float*);
using Uniform1iFn = void(APIENTRY*)(GLint, GLint);
using Uniform1fFn = void(APIENTRY*)(GLint, float);
using Uniform4fFn = void(APIENTRY*)(GLint, float, float, float, float);
using isVertexArrayFn = GLboolean(APIENTRY*)(GLuint);
using isProgramFn = GLboolean(APIENTRY*)(GLuint);
using GenerateMipmapFn = void(APIENTRY*)(GLenum);
using GenFramebuffersFn = void(APIENTRY*)(GLsizei, GLuint*);
using DeleteFramebuffersFn = void(APIENTRY*)(GLsizei, const GLuint*);
using BindFramebufferFn = void(APIENTRY*)(GLenum, GLuint);
using FramebufferTexture2DFn = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint);
using checkFramebufferStatusFn = GLenum(APIENTRY*)(GLenum);

// Yuklenen adresler. Surec boyunca tek bir openGL surucusu oldugu icin
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
    createShaderFn mCreateShader = nullptr;
    DeleteShaderFn mDeleteShader = nullptr;
    ShaderSourceFn mShaderSource = nullptr;
    CompileShaderFn mCompileShader = nullptr;
    getShaderivFn mGetShaderiv = nullptr;
    getShaderInfoLogFn mGetShaderInfoLog = nullptr;
    createProgramFn mCreateProgram = nullptr;
    DeleteProgramFn mDeleteProgram = nullptr;
    AttachShaderFn mAttachShader = nullptr;
    LinkProgramFn mLinkProgram = nullptr;
    getProgramivFn mGetProgramiv = nullptr;
    getProgramInfoLogFn mGetProgramInfoLog = nullptr;
    UseProgramFn mUseProgram = nullptr;
    getUniformLocationFn mGetUniformLocation = nullptr;
    UniformMatrix4fvFn mUniformMatrix4fv = nullptr;
    Uniform1iFn mUniform1i = nullptr;
    Uniform1fFn mUniform1f = nullptr;
    Uniform4fFn mUniform4f = nullptr;
    isVertexArrayFn mIsVertexArray = nullptr;
    isProgramFn mIsProgram = nullptr;
    GenerateMipmapFn mGenerateMipmap = nullptr;
    GenFramebuffersFn mGenFramebuffers = nullptr;
    DeleteFramebuffersFn mDeleteFramebuffers = nullptr;
    BindFramebufferFn mBindFramebuffer = nullptr;
    FramebufferTexture2DFn mFramebufferTexture2D = nullptr;
    checkFramebufferStatusFn mCheckFramebufferStatus = nullptr;
};

Functions gFunctions;
bool gLoaded = false;
void* gLastContext = nullptr;
unsigned int gContextGeneration = 0;

// Bir fonksiyonun adresini surucuden al. wglGetProcAddress hata durumunda
// nullptr disinda 1, 2, 3 veya -1 de donebilir; hepsi "yok" demek.
template <typename TFunction>
bool loadOne(const char* pName, TFunction& pOut, std::string& pMissing)
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

std::string glString(GLenum pName)
{
    const GLubyte* tText = glGetString(pName);
    return tText ? reinterpret_cast<const char*>(tText) : "?";
}

}

namespace Gl
{

bool load(std::string& pOutError)
{
    if (gLoaded)
    {
        return true;
    }

    // Surum: openGL 3.3 ve ustu gerekli (shader dili 3.30, vertex array)
    GLint tMajor = 0;
    GLint tMinor = 0;
    glGetIntegerv(kMajorVersion, &tMajor);
    glGetIntegerv(kMinorVersion, &tMinor);
    if (tMajor < 3 || (tMajor == 3 && tMinor < 3))
    {
        pOutError = "openGL 3.3 required, context is " + glString(GL_VERSION);
        return false;
    }

    std::string tMissing;
    Functions& tF = gFunctions;
    loadOne("glGenBuffers", tF.mGenBuffers, tMissing);
    loadOne("glDeleteBuffers", tF.mDeleteBuffers, tMissing);
    loadOne("glBindBuffer", tF.mBindBuffer, tMissing);
    loadOne("glBufferData", tF.mBufferData, tMissing);
    loadOne("glGenVertexArrays", tF.mGenVertexArrays, tMissing);
    loadOne("glDeleteVertexArrays", tF.mDeleteVertexArrays, tMissing);
    loadOne("glBindVertexArray", tF.mBindVertexArray, tMissing);
    loadOne("glEnableVertexAttribArray", tF.mEnableVertexAttribArray, tMissing);
    loadOne("glVertexAttribPointer", tF.mVertexAttribPointer, tMissing);
    loadOne("glCreateShader", tF.mCreateShader, tMissing);
    loadOne("glDeleteShader", tF.mDeleteShader, tMissing);
    loadOne("glShaderSource", tF.mShaderSource, tMissing);
    loadOne("glCompileShader", tF.mCompileShader, tMissing);
    loadOne("glGetShaderiv", tF.mGetShaderiv, tMissing);
    loadOne("glGetShaderInfoLog", tF.mGetShaderInfoLog, tMissing);
    loadOne("glCreateProgram", tF.mCreateProgram, tMissing);
    loadOne("glDeleteProgram", tF.mDeleteProgram, tMissing);
    loadOne("glAttachShader", tF.mAttachShader, tMissing);
    loadOne("glLinkProgram", tF.mLinkProgram, tMissing);
    loadOne("glGetProgramiv", tF.mGetProgramiv, tMissing);
    loadOne("glGetProgramInfoLog", tF.mGetProgramInfoLog, tMissing);
    loadOne("glUseProgram", tF.mUseProgram, tMissing);
    loadOne("glGetUniformLocation", tF.mGetUniformLocation, tMissing);
    loadOne("glUniformMatrix4fv", tF.mUniformMatrix4fv, tMissing);
    loadOne("glUniform1i", tF.mUniform1i, tMissing);
    loadOne("glUniform1f", tF.mUniform1f, tMissing);
    loadOne("glUniform4f", tF.mUniform4f, tMissing);
    loadOne("glIsVertexArray", tF.mIsVertexArray, tMissing);
    loadOne("glIsProgram", tF.mIsProgram, tMissing);
    loadOne("glGenerateMipmap", tF.mGenerateMipmap, tMissing);
    loadOne("glGenFramebuffers", tF.mGenFramebuffers, tMissing);
    loadOne("glDeleteFramebuffers", tF.mDeleteFramebuffers, tMissing);
    loadOne("glBindFramebuffer", tF.mBindFramebuffer, tMissing);
    loadOne("glFramebufferTexture2D", tF.mFramebufferTexture2D, tMissing);
    loadOne("glCheckFramebufferStatus", tF.mCheckFramebufferStatus, tMissing);
    if (!tMissing.empty())
    {
        pOutError = "openGL functions missing: " + tMissing;
        return false;
    }
    gLoaded = true;
    return true;
}

bool isLoaded()
{
    return gLoaded;
}

std::string describeContext()
{
    return glString(GL_VERSION) + " / " + glString(GL_RENDERER) + " / GLSL " + glString(kShadingLanguageVersion);
}

void* currentContext()
{
#if IBM
    return reinterpret_cast<void*>(wglGetCurrentContext());
#else
    return nullptr;
#endif
}

void noteContext()
{
    void* tContext = currentContext();
    if (tContext != gLastContext)
    {
        gLastContext = tContext;
        ++gContextGeneration;  // pop-out / pop-in: eski VAO'lar bu baglamda gecersiz
    }
}

unsigned int contextGeneration()
{
    return gContextGeneration;
}

GLboolean isVertexArray(GLuint pArray)
{
    return gFunctions.mIsVertexArray(pArray);
}

GLboolean isProgram(GLuint pProgram)
{
    return gFunctions.mIsProgram(pProgram);
}

void generateMipmap(GLenum pTarget)
{
    gFunctions.mGenerateMipmap(pTarget);
}

void genFramebuffers(GLsizei pCount, GLuint* pFramebuffers)
{
    gFunctions.mGenFramebuffers(pCount, pFramebuffers);
}

void deleteFramebuffers(GLsizei pCount, const GLuint* pFramebuffers)
{
    gFunctions.mDeleteFramebuffers(pCount, pFramebuffers);
}

void bindFramebuffer(GLenum pTarget, GLuint pFramebuffer)
{
    gFunctions.mBindFramebuffer(pTarget, pFramebuffer);
}

void framebufferTexture2D(GLenum pTarget, GLenum pAttachment, GLenum pTextureTarget, GLuint pTexture, GLint pLevel)
{
    gFunctions.mFramebufferTexture2D(pTarget, pAttachment, pTextureTarget, pTexture, pLevel);
}

GLenum checkFramebufferStatus(GLenum pTarget)
{
    return gFunctions.mCheckFramebufferStatus(pTarget);
}

void genBuffers(GLsizei pCount, GLuint* pBuffers)
{
    gFunctions.mGenBuffers(pCount, pBuffers);
}

void deleteBuffers(GLsizei pCount, const GLuint* pBuffers)
{
    gFunctions.mDeleteBuffers(pCount, pBuffers);
}

void bindBuffer(GLenum pTarget, GLuint pBuffer)
{
    gFunctions.mBindBuffer(pTarget, pBuffer);
}

void bufferData(GLenum pTarget, GLsizeiptr pSize, const void* pData, GLenum pUsage)
{
    gFunctions.mBufferData(pTarget, pSize, pData, pUsage);
}

void genVertexArrays(GLsizei pCount, GLuint* pArrays)
{
    gFunctions.mGenVertexArrays(pCount, pArrays);
}

void deleteVertexArrays(GLsizei pCount, const GLuint* pArrays)
{
    gFunctions.mDeleteVertexArrays(pCount, pArrays);
}

void bindVertexArray(GLuint pArray)
{
    gFunctions.mBindVertexArray(pArray);
}

void enableVertexAttribArray(GLuint pIndex)
{
    gFunctions.mEnableVertexAttribArray(pIndex);
}

void vertexAttribPointer(GLuint pIndex, GLint pSize, GLenum pType, GLboolean pNormalized, GLsizei pStride,
                         std::size_t pOffset)
{
    // openGL bu parametreyi isaretci olarak ister ama anlami "buffer icindeki bayt konumu"
    gFunctions.mVertexAttribPointer(pIndex, pSize, pType, pNormalized, pStride,
                                    reinterpret_cast<const void*>(pOffset));
}

GLuint createShader(GLenum pType)
{
    return gFunctions.mCreateShader(pType);
}

void deleteShader(GLuint pShader)
{
    gFunctions.mDeleteShader(pShader);
}

void shaderSource(GLuint pShader, const char* pSource)
{
    gFunctions.mShaderSource(pShader, 1, &pSource, nullptr);
}

void compileShader(GLuint pShader)
{
    gFunctions.mCompileShader(pShader);
}

GLint getShaderInt(GLuint pShader, GLenum pName)
{
    GLint tValue = 0;
    gFunctions.mGetShaderiv(pShader, pName, &tValue);
    return tValue;
}

std::string getShaderLog(GLuint pShader)
{
    std::vector<GLchar> tLog(static_cast<size_t>(getShaderInt(pShader, kInfoLogLength)) + 1, '\0');
    gFunctions.mGetShaderInfoLog(pShader, static_cast<GLsizei>(tLog.size()), nullptr, tLog.data());
    return tLog.data();
}

GLuint createProgram()
{
    return gFunctions.mCreateProgram();
}

void deleteProgram(GLuint pProgram)
{
    gFunctions.mDeleteProgram(pProgram);
}

void attachShader(GLuint pProgram, GLuint pShader)
{
    gFunctions.mAttachShader(pProgram, pShader);
}

void linkProgram(GLuint pProgram)
{
    gFunctions.mLinkProgram(pProgram);
}

GLint getProgramInt(GLuint pProgram, GLenum pName)
{
    GLint tValue = 0;
    gFunctions.mGetProgramiv(pProgram, pName, &tValue);
    return tValue;
}

std::string getProgramLog(GLuint pProgram)
{
    std::vector<GLchar> tLog(static_cast<size_t>(getProgramInt(pProgram, kInfoLogLength)) + 1, '\0');
    gFunctions.mGetProgramInfoLog(pProgram, static_cast<GLsizei>(tLog.size()), nullptr, tLog.data());
    return tLog.data();
}

void useProgram(GLuint pProgram)
{
    gFunctions.mUseProgram(pProgram);
}

GLint getUniformLocation(GLuint pProgram, const char* pName)
{
    return gFunctions.mGetUniformLocation(pProgram, pName);
}

void uniformMatrix4(GLint pLocation, const float pMatrix[16])
{
    gFunctions.mUniformMatrix4fv(pLocation, 1, GL_FALSE, pMatrix);
}

void uniform1i(GLint pLocation, GLint pValue)
{
    gFunctions.mUniform1i(pLocation, pValue);
}

void uniform1f(GLint pLocation, float pValue)
{
    gFunctions.mUniform1f(pLocation, pValue);
}

void uniform4(GLint pLocation, float pX, float pY, float pZ, float pW)
{
    gFunctions.mUniform4f(pLocation, pX, pY, pZ, pW);
}

}
