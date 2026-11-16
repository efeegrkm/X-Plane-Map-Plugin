// GPU'da KALICI sekil: Mercator koordinatlari bir kez yuklenir; kaydirma/zoom
// sadece iki uniform ile GPU'da yapilir (her karede yeniden gonderme FPS dusurur).
#pragma once

#include <vector>

#include "gfx/ContextVertexArray.h"
#include "gfx/Mesh.h"

namespace Gfx
{

enum class Primitive
{
    Triangles,
    Lines,  // 1 piksel
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

    // openGL baglami etkinken, ana thread'de
    void upload(const std::vector<ColorVertex>& pVertices, Primitive pPrimitive);

    bool isEmpty() const;
    int vertexCount() const;
    Primitive getPrimitive() const;
    // VAO su anki baglamda baglanir (pop-out'ta yeniden olusur; buffer
    // baglamlar arasi paylasildigi icin koseler yeniden yuklenmez).
    void bindVertexArray() const;

private:
    void release();

    mutable ContextVertexArray mArray;  // baglam basina; cizerken tembel olusturulur
    unsigned int mBuffer = 0;
    int mCount = 0;
    Primitive mPrimitive = Primitive::Triangles;
};

}
