// Ekran disi cizim yuzeyi: RGBA doku + ona cizen FBO. Harita dokuya cizilir,
// ekrana tek dortgen basilir. Pop-out: doku paylasilir, FBO paylasilmaz
// (baglam basina olusturulur, ContextVertexArray mantigi).
#ifndef PRIME_MAP_SRC_GFX_RENDERTARGET_H
#define PRIME_MAP_SRC_GFX_RENDERTARGET_H

#include "gfx/Texture.h"

namespace Gfx
{

class renderTarget
{
public:
    renderTarget() = default;
    ~renderTarget();

    renderTarget(const renderTarget&) = delete;
    renderTarget& operator=(const renderTarget&) = delete;

    // Basarisizsa (FBO tamamlanamadi) 0
    unsigned int prepare(int pWidth, int pHeight);

    const Texture& getTexture() const;
    int width() const;
    int height() const;

private:
    void releaseFramebuffer();

    Texture mTexture;
    int mWidth = 0;
    int mHeight = 0;
    unsigned int mFramebuffer = 0;
    void* mContext = nullptr;
    unsigned int mGeneration = 0;
};

}
#endif  // PRIME_MAP_SRC_GFX_RENDERTARGET_H
