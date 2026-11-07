// AtlasTextRenderer.h
// Kendi yazi cizicimiz (0.18): X-Plane'in Resources/fonts/DejaVuSans.ttf
// dosyasini okur, harfleri bir atlas dokusuna basar (gfx/FontAtlas) ve
// yazilari Canvas::QueueText ile biriktirir (bir karedeki butun etiketler
// tek cizim cagrisi).
//
// Boyut: Height() X-Plane yazisinin yuksekligiyle ayni (yerlesim ayni kalsin);
// atlas bu yuksekligin ekran pikseli karsiligiyla basilir (UI olcegi %150 ise
// 1,5 kat buyuk). Olcek degisince atlas yeniden basilir. Harfler tam piksele
// oturtulur: bulanik kenar olmaz.
#pragma once

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
    bool Load(const std::string& pFontPath, std::string* pError);

    // Her karede: ekran pikseli / boxel (UI olcegi). Degistiyse atlas yeniden basilir.
    void SetPixelsPerBoxel(double pPixelsPerBoxel);

    void Draw(int pX, int pY, std::string_view pText, const Rgba& pColor) override;
    int Width(std::string_view pText) override;
    int Height() override;

private:
    bool BuildAtlas(std::string* pError);

    Gfx::Canvas& mCanvas;
    int mBoxelHeight;
    double mPixelsPerBoxel = 1.0;
    std::vector<uint8_t> mFontData;
    Gfx::FontAtlas mAtlas;
    Gfx::Texture mTexture;     // atlasin dokusu (ilk cizimde / olcek degisince yuklenir)
    bool mTextureStale = true;
    std::vector<Gfx::TextVertex> mScratch;
};
