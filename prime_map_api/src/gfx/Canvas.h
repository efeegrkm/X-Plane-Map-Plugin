// Harita ve arayuzun tek cizim kapisi: ucgen agi, doku ve yazi cizimi.
#ifndef PRIME_MAP_SRC_GFX_CANVAS_H
#define PRIME_MAP_SRC_GFX_CANVAS_H

#include <string>
#include <vector>

#include "gfx/ContextVertexArray.h"
#include "gfx/Mesh.h"

namespace Gfx
{
class Texture;
class StaticMesh;
class renderTarget;

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

    bool init();
    bool isReady() const;
    const std::string& errorText() const;
    const std::string& contextText() const;

    void beginFrame();

    bool isContextUsable() const;

    void draw(const Mesh& pMesh);
    void draw(const OutlinedLines& pLines);
    void drawTextured(const Texture& pTexture, const std::vector<TexturedVertex>& pVertices, bool pBlend = true);
    void drawStatic(const StaticMesh& pMesh, const PointF& pOffset, double pScale, float pLineWidth = 1.0f);

    void queueText(const Texture& pAtlas, const std::vector<TextVertex>& pVertices);
    void flushText();

    bool beginOffscreen(renderTarget& pTarget, const RectD& pBoxels);
    void endOffscreen();

private:
    struct Program
    {
        unsigned int mId = 0;
        int mMvpLocation = -1;
        int mTransformLocation = -1;
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
    ContextVertexArray mColorArray;
    unsigned int mColorBuffer = 0;
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
#endif
