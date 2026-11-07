// FontAtlas.h
// Yazi tipi atlasi (0.18): bir TrueType dosyasinin harflerini tek bir RGBA
// goruntuye basar ve her harfin yerini / olcusunu tutar. OpenGL'e ve X-Plane'e
// bagimli DEGIL (testlerde calisir); dokuya yukleme AtlasTextRenderer'in isi.
//
//   +--------------------------------+   beyaz harfler, alfa = kaplama (0..255)
//   | A B C D E F G H I J K ...      |   raf (shelf) yerlesimi: harfler soldan
//   | a b c d e f ...                |   saga dizilir, satir dolunca alta gecilir
//   | 0 1 2 ... ğ ş ı İ ° ...        |
//   +--------------------------------+
//
// Boyut "buyuk harf yuksekligi" ile verilir: 'H' harfi tam pCapHeightPixels
// piksel olur. Boylece X-Plane'in yazi yuksekligine (XPLMGetFontDimensions)
// gore ayni gorunen boyut secilir.
//
// Harf seti: ASCII 32-126, Latin-1 (160-255: derece isareti, o, u, c ...) ve
// Turkce harfler (G/g breve, I noktali, i noktasiz, S/s cedilla). Metin UTF-8.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Gfx
{

struct GlyphInfo
{
    // Atlastaki yeri (0..1 doku koordinati, v = 0 ust satir)
    float mU0 = 0.0f;
    float mV0 = 0.0f;
    float mU1 = 0.0f;
    float mV1 = 0.0f;
    int mWidth = 0;    // bitmap boyu (piksel); bosluk karakterinde 0
    int mHeight = 0;
    int mLeft = 0;     // kalem konumundan bitmap'in sol kenarina (piksel)
    int mTop = 0;      // taban cizgisinden bitmap'in ust kenarina (piksel, yukari +)
    float mAdvance = 0.0f;  // sonraki harfe kalem ilerlemesi (piksel)
};

class FontAtlas
{
public:
    // pFontData: .ttf dosyasinin tamami (atlas olusurken okunur, sonra gerekmez)
    bool Build(const std::vector<uint8_t>& pFontData, float pCapHeightPixels, std::string* pError);

    bool IsBuilt() const;
    int Size() const;  // kare atlasin kenari (piksel)
    const std::vector<uint8_t>& Rgba() const;

    // Harf yoksa '?' doner; o da yoksa nullptr
    const GlyphInfo* Find(uint32_t pCodepoint) const;

    // Metnin piksel genisligi (ilerlemelerin toplami)
    float Measure(std::string_view pText) const;

    // UTF-8 cozucu: pIndex'teki harfi doner ve pIndex'i ilerletir. Bozuk bayt: '?'.
    static uint32_t NextCodepoint(std::string_view pText, size_t* pIndex);

private:
    int mSize = 0;
    std::vector<uint8_t> mRgba;
    std::unordered_map<uint32_t, GlyphInfo> mGlyphs;
};

}  // namespace Gfx
