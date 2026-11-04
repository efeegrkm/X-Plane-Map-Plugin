// Canvas.h
// Haritanin ve arayuzun TEK cizim kapisi: Mesh'leri (renkli ucgenler) ve
// dokulu seritleri modern OpenGL (vertex buffer + shader) ile cizer.
// Projede OpenGL cagrisi sadece gfx/ klasorundedir; katmanlar ve arayuz
// sadece Canvas'a "su Mesh'i ciz" der.
//
// Kullanim (her pencere ciziminde):
//   if (!mCanvas.Init()) { ...hata yazisi... }   // ilk karede bir kez yukler
//   mCanvas.BeginFrame();                        // X-Plane'in matrislerini okur
//   mCanvas.Draw(tMesh);
//
// X-Plane ile birlikte yasamak icin her Draw:
//   - X-Plane'in grafik durumunu XPLMSetGraphicsState ile bildirir,
//   - kendi shader / vertex buffer'ini baglar, cizer,
//   - X-Plane'in bagli tuttugu program / vertex array / buffer'i ve
//     yuz ayiklama (cull face) durumunu oldugu gibi geri koyar.
// Yazilar X-Plane'in XPLMDrawString'i ile cizilir (Ui::Text); Draw'lar
// arasinda guvenle cagrilabilir.
#pragma once

#include <string>
#include <vector>

#include "gfx/Mesh.h"

namespace Gfx
{

class Texture;
class StaticMesh;

class Canvas
{
public:
    Canvas() = default;
    ~Canvas();

    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;

    // OpenGL fonksiyonlarini yukler, shader'lari derler. Ilk cagride is
    // yapar, sonrakiler sadece sonucu doner. OpenGL baglami etkinken
    // (draw callback icinde) cagrilmali. Basarisizsa neden: ErrorText().
    bool Init();
    bool IsReady() const;
    const std::string& ErrorText() const;
    const std::string& ContextText() const;  // OpenGL surumu / kart (log icin)

    // Her cizimin basinda: boxel -> ekran matrisini o anki OpenGL'den okur
    void BeginFrame();

    void Draw(const Mesh& pMesh);
    void Draw(const OutlinedLines& pLines);  // once kontur, sonra renkli cizgi
    // pVertices: 3'erli ucgenler (doku koordinatli)
    void DrawTextured(const Texture& pTexture, const std::vector<TexturedVertex>& pVertices);
    // GPU'da duran sekli ciz: ekrandaki konum = pOffset + kose * pScale (boxel)
    void DrawStatic(const StaticMesh& pMesh, const PointF& pOffset, double pScale);

private:
    struct Program
    {
        unsigned int mId = 0;
        int mMvpLocation = -1;
        int mTransformLocation = -1;  // vec4(olcek x, olcek y, kaydirma x, kaydirma y)
    };

    bool BuildProgram(const char* pVertexSource, const char* pFragmentSource, Program& pOut);
    void Release();

    bool mTried = false;
    bool mReady = false;
    std::string mError;
    std::string mContext;

    Program mColorProgram;
    Program mTextureProgram;
    unsigned int mColorArray = 0;   // vertex array (koselerin duzeni)
    unsigned int mColorBuffer = 0;  // vertex buffer (koseler)
    unsigned int mTextureArray = 0;
    unsigned int mTextureBuffer = 0;
    float mMvp[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

}  // namespace Gfx
