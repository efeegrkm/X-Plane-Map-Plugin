// AtlasTextRenderer.cpp
#include "ui/AtlasTextRenderer.h"

#include <cmath>
#include <fstream>
#include <iterator>

#include "gfx/Canvas.h"

namespace
{

const double kRebuildTolerance = 0.01;  // UI olcegi bundan az degisirse atlas yeniden basilmaz

}  // namespace

AtlasTextRenderer::AtlasTextRenderer(Gfx::Canvas& pCanvas, int pBoxelHeight)
    : mCanvas(pCanvas)
    , mBoxelHeight(pBoxelHeight)
{
}

bool AtlasTextRenderer::Load(const std::string& pFontPath, std::string* pError)
{
    std::ifstream tFile(pFontPath, std::ios::binary);
    if (!tFile)
    {
        *pError = "font file not found: " + pFontPath;
        return false;
    }
    mFontData.assign(std::istreambuf_iterator<char>(tFile), std::istreambuf_iterator<char>());
    return BuildAtlas(pError);
}

bool AtlasTextRenderer::BuildAtlas(std::string* pError)
{
    const float tCapPixels = static_cast<float>(mBoxelHeight * mPixelsPerBoxel);
    if (!mAtlas.Build(mFontData, tCapPixels, pError))
    {
        return false;
    }
    mTextureStale = true;
    return true;
}

void AtlasTextRenderer::SetPixelsPerBoxel(double pPixelsPerBoxel)
{
    if (pPixelsPerBoxel <= 0.0 || std::abs(pPixelsPerBoxel - mPixelsPerBoxel) < kRebuildTolerance)
    {
        return;
    }
    mPixelsPerBoxel = pPixelsPerBoxel;
    std::string tError;
    BuildAtlas(&tError);  // basarisizsa eski atlas kalir (ayni dosya: olmamali)
}

void AtlasTextRenderer::Draw(int pX, int pY, std::string_view pText, const Rgba& pColor)
{
    if (!mAtlas.IsBuilt() || pText.empty())
    {
        return;
    }
    if (mTextureStale)
    {
        mTexture = Gfx::Texture::FromRgba(mAtlas.Size(), mAtlas.Rgba().data(), false);
        mTextureStale = false;
    }
    // Kalem piksel cinsinden ilerler; her harf tam piksele oturur, sonra boxel'e cevrilir
    const double tScale = mPixelsPerBoxel;
    double tPenX = std::round(pX * tScale);
    const double tBaseY = std::round(pY * tScale);
    mScratch.clear();
    size_t tIndex = 0;
    while (tIndex < pText.size())
    {
        const Gfx::GlyphInfo* tGlyph = mAtlas.Find(Gfx::FontAtlas::NextCodepoint(pText, &tIndex));
        if (!tGlyph)
        {
            continue;
        }
        if (tGlyph->mWidth > 0)
        {
            const double tLeftPixel = std::round(tPenX) + tGlyph->mLeft;
            const float tLeft = static_cast<float>(tLeftPixel / tScale);
            const float tRight = static_cast<float>((tLeftPixel + tGlyph->mWidth) / tScale);
            const float tTop = static_cast<float>((tBaseY + tGlyph->mTop) / tScale);
            const float tBottom = static_cast<float>((tBaseY + tGlyph->mTop - tGlyph->mHeight) / tScale);
            // Doku satir 0 = atlasin ustu (v0): harfin ust kenari
            const Gfx::TextVertex tTopLeft = {tLeft, tTop, tGlyph->mU0, tGlyph->mV0, pColor};
            const Gfx::TextVertex tTopRight = {tRight, tTop, tGlyph->mU1, tGlyph->mV0, pColor};
            const Gfx::TextVertex tBottomLeft = {tLeft, tBottom, tGlyph->mU0, tGlyph->mV1, pColor};
            const Gfx::TextVertex tBottomRight = {tRight, tBottom, tGlyph->mU1, tGlyph->mV1, pColor};
            mScratch.insert(mScratch.end(), {tTopLeft, tBottomLeft, tBottomRight, tTopLeft, tBottomRight, tTopRight});
        }
        tPenX += tGlyph->mAdvance;
    }
    mCanvas.QueueText(mTexture, mScratch);
}

int AtlasTextRenderer::Width(std::string_view pText)
{
    return static_cast<int>(std::ceil(mAtlas.Measure(pText) / mPixelsPerBoxel));
}

int AtlasTextRenderer::Height()
{
    return mBoxelHeight;
}
