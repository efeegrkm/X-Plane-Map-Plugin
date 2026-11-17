// Baglama bagli (context) vertex array (VAO) tutucusu. VAO'lar pop-out
// baglamlarina paylasilmaz; yanlis baglamdaki VAO'yu baglamak X-Plane'i
// coker. Baglam degistirse (Gl::ContextGeneration) VAO yeniden olusturulur.
#ifndef PRIME_MAP_SRC_GFX_CONTEXTVERTEXARRAY_H
#define PRIME_MAP_SRC_GFX_CONTEXTVERTEXARRAY_H

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

    // Su anki baglam icin gecerli VAO'yu verir; yeni olusturuldugunda pSetup
    // o VAO bagliyken cagrilir. Donuste VAO bagli kalir.
    unsigned int get(const std::function<void()>& pSetup);

    // Olusturuldugu baglam etkinse siler, degilse sadece unutur
    // (olu baglamin adini silmek gecersizdir).
    void release();

private:
    void* mContext = nullptr;
    unsigned int mGeneration = 0;
    unsigned int mArray = 0;
};

}
#endif  // PRIME_MAP_SRC_GFX_CONTEXTVERTEXARRAY_H
