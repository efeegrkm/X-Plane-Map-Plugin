// Ekran disi cizim yuzeyi: doku ve ona cizen framebuffer.
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
#endif
