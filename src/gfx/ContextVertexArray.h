// ContextVertexArray.h
// OpenGL baglamina (context) bagli vertex array (VAO) tutucusu.
//
// Neden: pencere pop-out yapilinca X-Plane onu ayri bir OpenGL baglaminda
// cizer. Vertex buffer, doku ve shader'lar baglamlar arasinda paylasilir ama
// VAO'lar paylasilmaz. Ana baglamda olusturulmus bir VAO'yu pop-out
// baglaminda baglamak gecersizdir; cizim rastgele bellek okur ve X-Plane
// coker. Bu sinif VAO'yu su anki baglam icin hazirlar: baglam degistiyse
// (Gl::ContextGeneration) yenisini olusturur ve duzenini pSetup ile kurar.
#pragma once

#include <functional>

namespace Gfx
{

class ContextVertexArray
{
public:
    ContextVertexArray() = default;
    ~ContextVertexArray();

    ContextVertexArray(ContextVertexArray&& pOther) noexcept;
    ContextVertexArray& operator=(ContextVertexArray&& pOther) noexcept;
    ContextVertexArray(const ContextVertexArray&) = delete;
    ContextVertexArray& operator=(const ContextVertexArray&) = delete;

    // Su anki baglam icin gecerli VAO'yu verir; gerekiyorsa olusturur. Yeni
    // VAO olusturuldugunda o VAO bagliyken pSetup cagrilir (buffer'i baglayip
    // koselerin duzenini tanimlar). Donuste VAO bagli kalir.
    unsigned int Get(const std::function<void()>& pSetup);

    // Su anki baglamda olusturulmussa VAO'yu siler, degilse sadece unutur
    // (olu bir baglamin adini silmeye calismak gecersizdir).
    void Release();

private:
    void* mContext = nullptr;
    unsigned int mGeneration = 0;
    unsigned int mArray = 0;
};

}  // namespace Gfx
