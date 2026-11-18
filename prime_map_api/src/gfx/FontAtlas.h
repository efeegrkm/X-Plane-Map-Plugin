// TrueType yazi tipini tek bir doku atlasina basar.
#ifndef PRIME_MAP_SRC_GFX_FONTATLAS_H
#define PRIME_MAP_SRC_GFX_FONTATLAS_H

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Gfx
{
struct GlyphInfo
{
    float mU0 = 0.0f;
    float mV0 = 0.0f;
    float mU1 = 0.0f;
    float mV1 = 0.0f;
    int mWidth = 0;
    int mHeight = 0;
    int mLeft = 0;
    int mTop = 0;
    float mAdvance = 0.0f;
};

class FontAtlas
{
public:
    bool build(const std::vector<uint8_t>& pFontData, float pCapHeightPixels, std::string* pError);

    bool isBuilt() const;
    int size() const;
    const std::vector<uint8_t>& Rgba() const;

    const GlyphInfo* find(uint32_t pCodepoint) const;

    float measure(std::string_view pText) const;

    static uint32_t nextCodepoint(std::string_view pText, size_t* pIndex);

private:
    int mSize = 0;
    std::vector<uint8_t> mRgba;
    std::unordered_map<uint32_t, GlyphInfo> mGlyphs;
};
}
#endif
