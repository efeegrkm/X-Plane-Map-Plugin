#include "gfx/StaticMesh.h"

#include <cstddef>
#include <utility>

#include "gfx/GlApi.h"

namespace
{

const GLenum kStaticDraw = 0x88E4;  // GL_STATIC_DRAW: bir kez yazilir, cok kez cizilir

}

namespace Gfx
{

StaticMesh::~StaticMesh()
{
    release();
}

StaticMesh::StaticMesh(StaticMesh&& pOther) noexcept
    : mArray(std::move(pOther.mArray))
    , mBuffer(pOther.mBuffer)
    , mCount(pOther.mCount)
    , mPrimitive(pOther.mPrimitive)
{
    pOther.mBuffer = 0;
    pOther.mCount = 0;
}

StaticMesh& StaticMesh::operator=(StaticMesh&& pOther) noexcept
{
    if (this != &pOther)
    {
        release();
        mArray = std::move(pOther.mArray);
        mBuffer = pOther.mBuffer;
        mCount = pOther.mCount;
        mPrimitive = pOther.mPrimitive;
        pOther.mBuffer = 0;
        pOther.mCount = 0;
    }
    return *this;
}

void StaticMesh::upload(const std::vector<ColorVertex>& pVertices, Primitive pPrimitive)
{
    release();
    mPrimitive = pPrimitive;
    mCount = static_cast<int>(pVertices.size());
    if (mCount == 0 || !Gl::isLoaded())
    {
        mCount = 0;
        return;
    }
    // X-Plane'in bagli tuttugu buffer'i bozmamak icin kaydet. VAO burada
    // OLUSTURULMAZ: cizim hangi baglamda olacaksa orada olusur.
    GLint tOldBuffer = 0;
    glGetIntegerv(Gl::kArrayBufferBinding, &tOldBuffer);
    Gl::genBuffers(1, &mBuffer);
    Gl::bindBuffer(Gl::kArrayBuffer, mBuffer);
    Gl::bufferData(Gl::kArrayBuffer, static_cast<GLsizeiptr>(pVertices.size() * sizeof(ColorVertex)),
                   pVertices.data(), kStaticDraw);
    Gl::bindBuffer(Gl::kArrayBuffer, static_cast<GLuint>(tOldBuffer));
}

void StaticMesh::bindVertexArray() const
{
    const unsigned int tBuffer = mBuffer;
    mArray.get(
        [tBuffer]()
        {
            Gl::bindBuffer(Gl::kArrayBuffer, tBuffer);
            Gl::enableVertexAttribArray(0);
            Gl::vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), offsetof(ColorVertex, mX));
            Gl::enableVertexAttribArray(1);
            Gl::vertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ColorVertex),
                                    offsetof(ColorVertex, mColor));
        });
}

bool StaticMesh::isEmpty() const
{
    return mCount == 0;
}

int StaticMesh::vertexCount() const
{
    return mCount;
}

Primitive StaticMesh::getPrimitive() const
{
    return mPrimitive;
}

void StaticMesh::release()
{
    if (mBuffer != 0 && Gl::isLoaded())
    {
        Gl::deleteBuffers(1, &mBuffer);
    }
    mArray.release();
    mBuffer = 0;
    mCount = 0;
}

}
