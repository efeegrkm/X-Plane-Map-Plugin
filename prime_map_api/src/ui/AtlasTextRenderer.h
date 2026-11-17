// Kendi yazi cizicimiz: X-Plane'in DejaVuSans.ttf'ini stb_truetype ile bir atlas
// dokusuna basar, yazilari Canvas::QueueText ile tek cizim cagrisinda basar.
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
    // pBoxelHeight: buyuk harf yuksekligi (boxel); X-Plane yazisininki verilir
    AtlasTextRenderer(Gfx::Canvas& pCanvas, int pBoxelHeight);

    // Yazi tipi dosyasini okur ve atlasi bir kez kurar (kurulamazsa false, neden pError)
    bool load(const std::string& pFontPath, std::string* pError);

    // Her karede: ekran pikseli / boxel (UI olcegi). Degistiyse atlas yeniden basilir.
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
    Gfx::Texture mTexture;     // atlasin dokusu (ilk cizimde / olcek degisince yuklenir)
    bool mTextureStale = true;
    std::vector<Gfx::TextVertex> mScratch;
};
#endif  // PRIME_MAP_SRC_UI_ATLASTEXTRENDERER_H
