// StaticMesh.h
// GPU'da KALICI duran sekil (RAII): bir kez yuklenir, her karede sadece bir
// olcek + kaydirma ile cizilir (Canvas::DrawStatic).
//
// Neden: buyuk bir havalimani (KORD) ~200 bin nokta. Her karede bunlari
// lat/lon'dan ekrana cevirip GPU'ya yeniden gondermek FPS'i dusurur.
// Web Mercator'da ekrana cevirme dogrusal (olcek + kaydirma) oldugu icin
// noktalar bir kez Mercator koordinatinda yuklenir; kaydirma / zoom
// degistikce sadece iki sayi (uniform) degisir, donusumu GPU yapar.
#pragma once

#include <vector>

#include "gfx/Mesh.h"

namespace Gfx
{

enum class Primitive
{
    Triangles,  // her 3 kose bir ucgen
    Lines,      // her 2 kose bir cizgi (1 piksel)
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

    // Koseleri GPU'ya yukler (OpenGL baglami etkinken, ana thread'de)
    void Upload(const std::vector<ColorVertex>& pVertices, Primitive pPrimitive);

    bool IsEmpty() const;
    int VertexCount() const;
    Primitive GetPrimitive() const;
    unsigned int VertexArray() const;  // Canvas icin

private:
    void Release();

    unsigned int mArray = 0;
    unsigned int mBuffer = 0;
    int mCount = 0;
    Primitive mPrimitive = Primitive::Triangles;
};

}  // namespace Gfx
