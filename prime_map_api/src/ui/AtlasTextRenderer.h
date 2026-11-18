// Yazilari font atlasi ile tek cizim cagrisinda basan yazi cizici.
#ifndef PRIME_MAP_SRC_UI_ATLASTEXTRENDERER_H
#define PRIME_MAP_SRC_UI_ATLASTEXTRENDERER_H

#include <cstdint>
#include <string>
#include <vector>

#include "gfx/Canvas.h"
#include "gfx/FontAtlas.h"
#include "gfx/Texture.h"
#include "ui/TextRenderer.h"

class AtlasTextRenderer : public TextRenderer
{
public:
    AtlasTextRenderer(Gfx::Canvas& pCanvas, int pBoxelHeight);

    bool load(const std::string& pFontPath, std::string* pError);

    void setPixelsPerBoxel(double pPixelsPerBoxel);

    void draw(int pX, int pY, std::string_view pText, const Rgba& pColor) override;
    int width(std::string_view pText) override;
    int height() override;

private:
    bool buildAtlas(std::string* pError);

    Gfx::Canvas& mCanvas;
    int mBoxelHeight;
    double mPixelsPerBoxel = 1.0;
    std::vector<uint8_t> mFontData;
    Gfx::FontAtlas mAtlas;
    Gfx::Texture mTexture;
    bool mTextureStale = true;
    std::vector<Gfx::TextVertex> mScratch;
};
#endif
