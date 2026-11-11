// FontAtlas.cpp
#include "gfx/FontAtlas.h"

#include <algorithm>
#include <cmath>

#if defined(_MSC_VER)
#pragma warning(push, 0)  // dis kutuphane: kendi uyarilarimiz /W4 ile temiz kalsin
#endif
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb/stb_truetype.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace
{

const int kPadding = 1;            // harfler arasi bos piksel (dokuda komsu harf sizmasin)
const int kMaxAtlasSize = 2048;

// Atlasa basilacak harfler
std::vector<uint32_t> Codepoints()
{
    std::vector<uint32_t> tList;
    for (uint32_t tCode = 32; tCode <= 126; ++tCode)
    {
        tList.push_back(tCode);
    }
    for (uint32_t tCode = 160; tCode <= 255; ++tCode)
    {
        tList.push_back(tCode);
    }
    for (uint32_t tCode : {0x011Eu, 0x011Fu, 0x0130u, 0x0131u, 0x015Eu, 0x015Fu})  // G g I i S s (Turkce)
    {
        tList.push_back(tCode);
    }
    return tList;
}

}  // namespace

namespace Gfx
{

bool FontAtlas::Build(const std::vector<uint8_t>& pFontData, float pCapHeightPixels, std::string* pError)
{
    mGlyphs.clear();
    mRgba.clear();
    mSize = 0;
    stbtt_fontinfo tFont;
    if (pFontData.empty() ||
        stbtt_InitFont(&tFont, pFontData.data(), stbtt_GetFontOffsetForIndex(pFontData.data(), 0)) == 0)
    {
        *pError = "font file could not be read";
        return false;
    }
    // Olcek: 'H' harfinin yuksekligi pCapHeightPixels olsun
    int tX0 = 0;
    int tY0 = 0;
    int tX1 = 0;
    int tY1 = 0;
    if (stbtt_GetCodepointBox(&tFont, 'H', &tX0, &tY0, &tX1, &tY1) == 0 || tY1 <= 0)
    {
        *pError = "font has no 'H' glyph";
        return false;
    }
    const float tScale = pCapHeightPixels / static_cast<float>(tY1);

    // Once harflerin bitmap'leri, sonra yerlesim (kucuk atlastan baslayip sigmazsa buyutulur)
    struct Bitmap
    {
        uint32_t mCode;
        std::vector<uint8_t> mPixels;
        GlyphInfo mInfo;
    };
    std::vector<Bitmap> tBitmaps;
    for (uint32_t tCode : Codepoints())
    {
        const int tGlyph = stbtt_FindGlyphIndex(&tFont, static_cast<int>(tCode));
        if (tGlyph == 0 && tCode != ' ')
        {
            continue;  // yazi tipinde yok
        }
        Bitmap tBitmap;
        tBitmap.mCode = tCode;
        int tAdvance = 0;
        int tBearing = 0;
        stbtt_GetGlyphHMetrics(&tFont, tGlyph, &tAdvance, &tBearing);
        tBitmap.mInfo.mAdvance = tAdvance * tScale;
        int tLeft = 0;
        int tTop = 0;
        int tRight = 0;
        int tBottom = 0;
        stbtt_GetGlyphBitmapBox(&tFont, tGlyph, tScale, tScale, &tLeft, &tTop, &tRight, &tBottom);
        tBitmap.mInfo.mWidth = std::max(0, tRight - tLeft);
        tBitmap.mInfo.mHeight = std::max(0, tBottom - tTop);
        tBitmap.mInfo.mLeft = tLeft;
        tBitmap.mInfo.mTop = -tTop;  // stb: y asagi +; bizde taban cizgisinden yukari +
        if (tBitmap.mInfo.mWidth > 0 && tBitmap.mInfo.mHeight > 0)
        {
            tBitmap.mPixels.resize(static_cast<size_t>(tBitmap.mInfo.mWidth) * tBitmap.mInfo.mHeight);
            stbtt_MakeGlyphBitmap(&tFont, tBitmap.mPixels.data(), tBitmap.mInfo.mWidth, tBitmap.mInfo.mHeight,
                                  tBitmap.mInfo.mWidth, tScale, tScale, tGlyph);
        }
        tBitmaps.push_back(std::move(tBitmap));
    }

    // Raf yerlesimi
    for (int tSize = 128; tSize <= kMaxAtlasSize; tSize *= 2)
    {
        int tPenX = kPadding;
        int tPenY = kPadding;
        int tRowHeight = 0;
        bool tFits = true;
        for (Bitmap& tBitmap : tBitmaps)
        {
            const int tWidth = tBitmap.mInfo.mWidth;
            const int tHeight = tBitmap.mInfo.mHeight;
            if (tPenX + tWidth + kPadding > tSize)
            {
                tPenX = kPadding;
                tPenY += tRowHeight + kPadding;
                tRowHeight = 0;
            }
            if (tPenY + tHeight + kPadding > tSize)
            {
                tFits = false;
                break;
            }
            tBitmap.mInfo.mU0 = static_cast<float>(tPenX) / tSize;
            tBitmap.mInfo.mV0 = static_cast<float>(tPenY) / tSize;
            tBitmap.mInfo.mU1 = static_cast<float>(tPenX + tWidth) / tSize;
            tBitmap.mInfo.mV1 = static_cast<float>(tPenY + tHeight) / tSize;
            tPenX += tWidth + kPadding;
            tRowHeight = std::max(tRowHeight, tHeight);
        }
        if (!tFits)
        {
            continue;
        }
        mSize = tSize;
        mRgba.assign(static_cast<size_t>(tSize) * tSize * 4, 0);
        for (const Bitmap& tBitmap : tBitmaps)
        {
            const int tStartX = static_cast<int>(std::lround(tBitmap.mInfo.mU0 * tSize));
            const int tStartY = static_cast<int>(std::lround(tBitmap.mInfo.mV0 * tSize));
            for (int tRow = 0; tRow < tBitmap.mInfo.mHeight; ++tRow)
            {
                for (int tColumn = 0; tColumn < tBitmap.mInfo.mWidth; ++tColumn)
                {
                    const size_t tOut = (static_cast<size_t>(tStartY + tRow) * tSize + tStartX + tColumn) * 4;
                    mRgba[tOut + 0] = 255;
                    mRgba[tOut + 1] = 255;
                    mRgba[tOut + 2] = 255;
                    mRgba[tOut + 3] = tBitmap.mPixels[static_cast<size_t>(tRow) * tBitmap.mInfo.mWidth + tColumn];
                }
            }
            mGlyphs[tBitmap.mCode] = tBitmap.mInfo;
        }
        return true;
    }
    *pError = "glyphs do not fit into the atlas";
    return false;
}

bool FontAtlas::IsBuilt() const
{
    return mSize > 0;
}

int FontAtlas::Size() const
{
    return mSize;
}

const std::vector<uint8_t>& FontAtlas::Rgba() const
{
    return mRgba;
}

const GlyphInfo* FontAtlas::Find(uint32_t pCodepoint) const
{
    auto tFound = mGlyphs.find(pCodepoint);
    if (tFound == mGlyphs.end())
    {
        tFound = mGlyphs.find('?');
    }
    return tFound == mGlyphs.end() ? nullptr : &tFound->second;
}

float FontAtlas::Measure(std::string_view pText) const
{
    float tWidth = 0.0f;
    size_t tIndex = 0;
    while (tIndex < pText.size())
    {
        const GlyphInfo* tGlyph = Find(NextCodepoint(pText, &tIndex));
        if (tGlyph)
        {
            tWidth += tGlyph->mAdvance;
        }
    }
    return tWidth;
}

uint32_t FontAtlas::NextCodepoint(std::string_view pText, size_t* pIndex)
{
    const auto tByte = [&pText](size_t pAt)
    {
        return static_cast<uint8_t>(pText[pAt]);
    };
    const uint8_t tFirst = tByte(*pIndex);
    int tLength = 1;
    uint32_t tCode = tFirst;
    if (tFirst >= 0xF0)
    {
        tLength = 4;
        tCode = tFirst & 0x07u;
    }
    else if (tFirst >= 0xE0)
    {
        tLength = 3;
        tCode = tFirst & 0x0Fu;
    }
    else if (tFirst >= 0xC0)
    {
        tLength = 2;
        tCode = tFirst & 0x1Fu;
    }
    else if (tFirst >= 0x80)
    {
        ++*pIndex;
        return '?';  // devam bayti ile baslayan bozuk dizi
    }
    if (*pIndex + tLength > pText.size())
    {
        *pIndex = pText.size();
        return '?';
    }
    for (int tNext = 1; tNext < tLength; ++tNext)
    {
        const uint8_t tContinuation = tByte(*pIndex + tNext);
        if ((tContinuation & 0xC0u) != 0x80u)
        {
            ++*pIndex;
            return '?';
        }
        tCode = (tCode << 6) | (tContinuation & 0x3Fu);
    }
    *pIndex += tLength;
    return tCode;
}

}  // namespace Gfx
