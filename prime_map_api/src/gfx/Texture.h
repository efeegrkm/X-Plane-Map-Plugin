// X-Plane kurali: doku numarasi XPLMGenerateTextureNumbers ile alinir, baglama
// XPLMBindTexture2d ile yapilir. Olusturma/silme ana thread'de, GL baglami
// etkinken yapilmali.
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

    // Satir 0 = dokunun v = 0 kenari. pMipmaps: bellek 4/3 kat ama titreme olmaz.
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
#endif  // PRIME_MAP_SRC_GFX_TEXTURE_H
