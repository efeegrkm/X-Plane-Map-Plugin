// X-Plane kurallarina uygun doku nesnesi.
#ifndef PRIME_MAP_SRC_GFX_TEXTURE_H
#define PRIME_MAP_SRC_GFX_TEXTURE_H

#include <cstdint>

namespace Gfx
{
class Texture
{
public:
    Texture() = default;
    ~Texture();

    Texture(Texture&& pOther) noexcept;
    Texture& operator=(Texture&& pOther) noexcept;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    static Texture fromRgba(int pSize, const uint8_t* pRgba, bool pMipmaps = false);

    static Texture createEmpty(int pWidth, int pHeight);

    bool isValid() const;
    int id() const;

private:
    explicit Texture(int pId);
    void release();

    int mId = 0;
};
}
#endif
