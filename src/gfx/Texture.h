// Texture.h
// OpenGL dokusu (RAII): nesne yok olunca doku da silinir. Kopyalanamaz,
// tasinabilir (std::move ile bir tablodan digerine verilebilir).
//
// X-Plane kurali: doku numarasi XPLMGenerateTextureNumbers ile alinir,
// baglamak icin XPLMBindTexture2d kullanilir (dogrudan glBindTexture degil;
// X-Plane bagli dokuyu kendi onbelleginde tutar).
// Olusturma ve silme ana thread'de, OpenGL baglami etkinken yapilmali.
#pragma once

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

    // pSize x pSize RGBA goruntuden doku (satir 0 = dokunun v = 0 kenari).
    // pMipmaps: kucultulmus seviyeler de uretilir (uzaktan bakinca kumlanma /
    // titreme olmaz; bellek 4/3 kat). Zemin dokulari bunu kullanir.
    static Texture FromRgba(int pSize, const uint8_t* pRgba, bool pMipmaps = false);

    bool IsValid() const;
    int Id() const;  // XPLMBindTexture2d'ye verilen numara

private:
    explicit Texture(int pId);
    void Release();

    int mId = 0;
};

}  // namespace Gfx
