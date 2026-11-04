// StaticMesh.cpp
#include "gfx/StaticMesh.h"

#include <cstddef>

#include "gfx/GlApi.h"

namespace
{

const GLenum kStaticDraw = 0x88E4;  // GL_STATIC_DRAW: bir kez yazilir, cok kez cizilir

}  // namespace

namespace Gfx
{

StaticMesh::~StaticMesh()
{
    Release();
}

StaticMesh::StaticMesh(StaticMesh&& pOther) noexcept
    : mArray(pOther.mArray)
    , mBuffer(pOther.mBuffer)
    , mCount(pOther.mCount)
    , mPrimitive(pOther.mPrimitive)
{
    pOther.mArray = 0;
    pOther.mBuffer = 0;
    pOther.mCount = 0;
}

StaticMesh& StaticMesh::operator=(StaticMesh&& pOther) noexcept
{
    if (this != &pOther)
    {
        Release();
        mArray = pOther.mArray;
        mBuffer = pOther.mBuffer;
        mCount = pOther.mCount;
        mPrimitive = pOther.mPrimitive;
        pOther.mArray = 0;
        pOther.mBuffer = 0;
        pOther.mCount = 0;
    }
    return *this;
}

void StaticMesh::Upload(const std::vector<ColorVertex>& pVertices, Primitive pPrimitive)
{
    Release();
    mPrimitive = pPrimitive;
    mCount = static_cast<int>(pVertices.size());
    if (mCount == 0 || !Gl::IsLoaded())
    {
        mCount = 0;
        return;
    }
    // X-Plane'in bagli tuttugu vertex array / buffer'i bozmamak icin kaydet
    GLint tOldArray = 0;
    GLint tOldBuffer = 0;
    glGetIntegerv(Gl::kVertexArrayBinding, &tOldArray);
    glGetIntegerv(Gl::kArrayBufferBinding, &tOldBuffer);

    Gl::GenVertexArrays(1, &mArray);
    Gl::GenBuffers(1, &mBuffer);
    Gl::BindVertexArray(mArray);
    Gl::BindBuffer(Gl::kArrayBuffer, mBuffer);
    Gl::BufferData(Gl::kArrayBuffer, static_cast<GLsizeiptr>(pVertices.size() * sizeof(ColorVertex)),
                   pVertices.data(), kStaticDraw);
    Gl::EnableVertexAttribArray(0);
    Gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), offsetof(ColorVertex, mX));
    Gl::EnableVertexAttribArray(1);
    Gl::VertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), offsetof(ColorVertex, mColor));

    Gl::BindVertexArray(static_cast<GLuint>(tOldArray));
    Gl::BindBuffer(Gl::kArrayBuffer, static_cast<GLuint>(tOldBuffer));
}

bool StaticMesh::IsEmpty() const
{
    return mCount == 0;
}

int StaticMesh::VertexCount() const
{
    return mCount;
}

Primitive StaticMesh::GetPrimitive() const
{
    return mPrimitive;
}

unsigned int StaticMesh::VertexArray() const
{
    return mArray;
}

void StaticMesh::Release()
{
    if (mBuffer != 0 && Gl::IsLoaded())
    {
        Gl::DeleteBuffers(1, &mBuffer);
        Gl::DeleteVertexArrays(1, &mArray);
    }
    mBuffer = 0;
    mArray = 0;
    mCount = 0;
}

}  // namespace Gfx
