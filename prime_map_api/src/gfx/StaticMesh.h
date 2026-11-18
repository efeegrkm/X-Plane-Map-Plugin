// GPU'da kalici sekil; kaydirma ve olcek her karede yalnizca uniform ile verilir.
#ifndef PRIME_MAP_SRC_GFX_STATICMESH_H
#define PRIME_MAP_SRC_GFX_STATICMESH_H

#include <vector>

#include "gfx/ContextVertexArray.h"
#include "gfx/Mesh.h"

namespace Gfx
{
enum class Primitive
{
    Triangles,
    Lines,
};

class StaticMesh
{
public:
    StaticMesh() = default;
    ~StaticMesh();

    StaticMesh(StaticMesh&& pOther) noexcept;
    StaticMesh& operator=(StaticMesh&& pOther) noexcept;
    StaticMesh(const StaticMesh&) = delete;
    StaticMesh& operator=(const StaticMesh&) = delete;

    void upload(const std::vector<ColorVertex>& pVertices, Primitive pPrimitive);

    bool isEmpty() const;
    int vertexCount() const;
    Primitive getPrimitive() const;
    void bindVertexArray() const;

private:
    void release();

    mutable ContextVertexArray mArray;
    unsigned int mBuffer = 0;
    int mCount = 0;
    Primitive mPrimitive = Primitive::Triangles;
};
}
#endif
