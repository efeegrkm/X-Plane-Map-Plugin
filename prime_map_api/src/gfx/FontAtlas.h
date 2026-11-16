// TrueType atlasi: harfler tek RGBA goruntuye basilir (raf yerlesimi, beyaz
// harf + alfa = kaplama). openGL/X-Plane'den bagimsiz, testlerde calisir.
// Boyut 'H' buyuk harf yuksekligi (pCapHeightPixels) ile verilir; metin UTF-8.
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
    // pFontData: .ttf dosyasinin tamami
    bool build(const std::vector<uint8_t>& pFontData, float pCapHeightPixels, std::string* pError);

    bool isBuilt() const;
    int size() const;  // kare atlasin kenari (piksel)
    const std::vector<uint8_t>& Rgba() const;

    // Harf yoksa '?' doner; o da yoksa nullptr
    const GlyphInfo* find(uint32_t pCodepoint) const;

    float measure(std::string_view pText) const;

    // Bozuk bayt: '?'
    static uint32_t nextCodepoint(std::string_view pText, size_t* pIndex);

private:
    int mSize = 0;
    std::vector<uint8_t> mRgba;
    std::unordered_map<uint32_t, GlyphInfo> mGlyphs;
};

}
