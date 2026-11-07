// RenderTarget.h
// Ekran disi cizim yuzeyi (0.18): bir RGBA doku + ona cizen framebuffer (FBO).
// Harita bu dokuya cizilir, her karede ekrana tek bir dortgen olarak basilir
// (MapImageCache).
//
// Pop-out: doku baglamlar arasi PAYLASILIR, FBO nesnesi PAYLASILMAZ (VAO gibi).
// Bu yuzden FBO, ContextVertexArray mantigiyla baglam basina olusturulur:
// baglam degistiyse (Gl::ContextGeneration) eskisi unutulur, yenisi ayni dokuya baglanir.
#pragma once

#include "gfx/Texture.h"

namespace Gfx
{

class RenderTarget
{
public:
    RenderTarget() = default;
    ~RenderTarget();

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    // Dokuyu pWidth x pHeight pikselde hazirlar (boyut degistiyse yeniden olusturur)
    // ve su anki baglamin FBO'sunu verir. Basarisizsa (FBO tamamlanamadi) 0.
    unsigned int Prepare(int pWidth, int pHeight);

    const Texture& GetTexture() const;
    int Width() const;
    int Height() const;

private:
    void ReleaseFramebuffer();

    Texture mTexture;
    int mWidth = 0;
    int mHeight = 0;
    unsigned int mFramebuffer = 0;
    void* mContext = nullptr;
    unsigned int mGeneration = 0;
};

}  // namespace Gfx
