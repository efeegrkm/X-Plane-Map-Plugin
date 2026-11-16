// Haritanin ve arayuzun TEK cizim kapisi: Mesh ve dokulu seritleri modern
// openGL ile cizer; projede openGL cagrisi sadece gfx/ klasorundedir.
#pragma once

#include <string>
#include <vector>

#include "gfx/ContextVertexArray.h"
#include "gfx/Mesh.h"

namespace Gfx
{

class Texture;
class StaticMesh;
class renderTarget;

// Yazi koseleri: konum (boxel), atlas koordinati, renk (alfa = atlasin kaplamasiyla carpilir)
struct TextVertex
{
    float mX = 0.0f;
    float mY = 0.0f;
    float mU = 0.0f;
    float mV = 0.0f;
    Rgba mColor;
};

class Canvas
{
public:
    Canvas() = default;
    ~Canvas();

    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;

    // openGL baglami etkinken (draw callback icinde) cagrilmali; ilk cagride yukler.
    bool init();
    bool isReady() const;
    const std::string& errorText() const;
    const std::string& contextText() const;

    void beginFrame();

    // Pop-out baglamini paylasimsiz acarsa false: cokmek yerine bos pencere + log.
    bool isContextUsable() const;

    void draw(const Mesh& pMesh);
    void draw(const OutlinedLines& pLines);  // once kontur, sonra renkli cizgi
    void drawTextured(const Texture& pTexture, const std::vector<TexturedVertex>& pVertices, bool pBlend = true);
    // konum = pOffset + kose * pScale (boxel); pLineWidth piksel kalinligi
    void drawStatic(const StaticMesh& pMesh, const PointF& pOffset, double pScale, float pLineWidth = 1.0f);

    void queueText(const Texture& pAtlas, const std::vector<TextVertex>& pVertices);
    // Her pencere ciziminin SONUNDA cagrilmali (sekil cizimleri once kendisi cagirir)
    void flushText();

    // X-Plane'in framebuffer/viewport/scissor durumu kaydedilir; FBO kurulamazsa false
    bool beginOffscreen(renderTarget& pTarget, const RectD& pBoxels);
    void endOffscreen();

private:
    struct Program
    {
        unsigned int mId = 0;
        int mMvpLocation = -1;
        int mTransformLocation = -1;  // vec4(olcek x, olcek y, kaydirma x, kaydirma y)
    };

    bool buildProgram(const char* pVertexSource, const char* pFragmentSource, Program& pOut);
    void release();

    bool canDraw() const;
    void bindColorArray();
    void bindTextureArray();
    void bindTextArray();

    bool mTried = false;
    bool mReady = false;
    std::string mError;
    std::string mContext;

    Program mColorProgram;
    Program mTextureProgram;
    Program mTextProgram;
    bool mContextUsable = false;
    ContextVertexArray mColorArray;  // baglam basina
    unsigned int mColorBuffer = 0;   // baglamlar arasi paylasilir
    ContextVertexArray mTextureArray;
    unsigned int mTextureBuffer = 0;
    ContextVertexArray mTextArray;
    unsigned int mTextBuffer = 0;
    std::vector<TextVertex> mPendingText;
    int mPendingAtlas = 0;
    float mMvp[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    bool mOffscreen = false;
    float mSavedMvp[16] = {};
    int mSavedDrawFramebuffer = 0;
    int mSavedReadFramebuffer = 0;
    int mSavedViewport[4] = {};
    bool mSavedScissor = false;
    float mSavedClearColor[4] = {};
};

}
