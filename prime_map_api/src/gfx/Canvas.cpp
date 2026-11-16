#include "gfx/Canvas.h"

#include <cstddef>

#include "XPLMGraphics.h"

#include "gfx/GlApi.h"
#include "gfx/renderTarget.h"
#include "gfx/StaticMesh.h"
#include "gfx/Texture.h"

namespace
{

const char* kColorVertexShader = R"(#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec4 aColor;
uniform mat4 uMvp;
uniform vec4 uTransform;
out vec4 vColor;
void main()
{
    vColor = aColor;
    gl_Position = uMvp * vec4(aPosition * uTransform.xy + uTransform.zw, 0.0, 1.0);
}
)";

const char* kColorFragmentShader = R"(#version 330 core
in vec4 vColor;
out vec4 oColor;
void main()
{
    oColor = vColor;
}
)";

const char* kTextureVertexShader = R"(#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aUv;
uniform mat4 uMvp;
out vec2 vUv;
void main()
{
    vUv = aUv;
    gl_Position = uMvp * vec4(aPosition, 0.0, 1.0);
}
)";

const char* kTextureFragmentShader = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uTexture;
out vec4 oColor;
void main()
{
    oColor = texture(uTexture, vUv);
}
)";

const char* kTextVertexShader = R"(#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColor;
uniform mat4 uMvp;
out vec2 vUv;
out vec4 vColor;
void main()
{
    vUv = aUv;
    vColor = aColor;
    gl_Position = uMvp * vec4(aPosition, 0.0, 1.0);
}
)";

const char* kTextFragmentShader = R"(#version 330 core
in vec2 vUv;
in vec4 vColor;
uniform sampler2D uTexture;
out vec4 oColor;
void main()
{
    oColor = vec4(vColor.rgb, vColor.a * texture(uTexture, vUv).a);
}
)";

// X-Plane'in bagli tuttugu nesneler cizimden once kaydedilip geri konur
class BindingGuard
{
public:
    BindingGuard()
    {
        glGetIntegerv(Gl::kCurrentProgram, &mProgram);
        glGetIntegerv(Gl::kVertexArrayBinding, &mArray);
        glGetIntegerv(Gl::kArrayBufferBinding, &mBuffer);
        // X-Plane bazen yuz ayiklamayi acik birakiyor: o zaman ters yonde
        // dizilmis ucgenler hic gorunmez. Cizim boyunca kapat.
        mCullFace = glIsEnabled(GL_CULL_FACE);
        if (mCullFace)
        {
            glDisable(GL_CULL_FACE);
        }
    }

    ~BindingGuard()
    {
        Gl::bindVertexArray(static_cast<GLuint>(mArray));
        Gl::bindBuffer(Gl::kArrayBuffer, static_cast<GLuint>(mBuffer));
        Gl::useProgram(static_cast<GLuint>(mProgram));
        if (mCullFace)
        {
            glEnable(GL_CULL_FACE);
        }
    }

    BindingGuard(const BindingGuard&) = delete;
    BindingGuard& operator=(const BindingGuard&) = delete;

private:
    GLint mProgram = 0;
    GLint mArray = 0;
    GLint mBuffer = 0;
    GLboolean mCullFace = GL_FALSE;
};

// sutun oncelikli
void multiply(const float pLeft[16], const float pRight[16], float pOut[16])
{
    for (int tColumn = 0; tColumn < 4; ++tColumn)
    {
        for (int tRow = 0; tRow < 4; ++tRow)
        {
            float tSum = 0.0f;
            for (int tK = 0; tK < 4; ++tK)
            {
                tSum += pLeft[tK * 4 + tRow] * pRight[tColumn * 4 + tK];
            }
            pOut[tColumn * 4 + tRow] = tSum;
        }
    }
}

GLuint compileShader(GLenum pType, const char* pSource, std::string& pError)
{
    const GLuint tShader = Gl::createShader(pType);
    Gl::shaderSource(tShader, pSource);
    Gl::compileShader(tShader);
    if (Gl::getShaderInt(tShader, Gl::kCompileStatus) == 0)
    {
        pError = "shader compile failed: " + Gl::getShaderLog(tShader);
        Gl::deleteShader(tShader);
        return 0;
    }
    return tShader;
}

}

namespace Gfx
{

Canvas::~Canvas()
{
    release();
}

bool Canvas::init()
{
    if (mTried)
    {
        return mReady;
    }
    mTried = true;
    if (!Gl::load(mError))
    {
        return false;
    }
    mContext = Gl::describeContext();
    if (!buildProgram(kColorVertexShader, kColorFragmentShader, mColorProgram) ||
        !buildProgram(kTextureVertexShader, kTextureFragmentShader, mTextureProgram) ||
        !buildProgram(kTextVertexShader, kTextFragmentShader, mTextProgram))
    {
        return false;
    }

    // Buffer'lar baglamlar arasi paylasilir; VAO'lar ilk cizimde o anki
    // baglam icin olusturulur (BindColorArray).
    Gl::genBuffers(1, &mColorBuffer);
    Gl::genBuffers(1, &mTextureBuffer);
    Gl::genBuffers(1, &mTextBuffer);

    BindingGuard tGuard;
    Gl::useProgram(mTextureProgram.mId);
    Gl::uniform1i(Gl::getUniformLocation(mTextureProgram.mId, "uTexture"), 0);
    Gl::useProgram(mTextProgram.mId);
    Gl::uniform1i(Gl::getUniformLocation(mTextProgram.mId, "uTexture"), 0);

    mReady = true;
    return true;
}

bool Canvas::isReady() const
{
    return mReady;
}

const std::string& Canvas::errorText() const
{
    return mError;
}

const std::string& Canvas::contextText() const
{
    return mContext;
}

bool Canvas::buildProgram(const char* pVertexSource, const char* pFragmentSource, Program& pOut)
{
    const GLuint tVertex = compileShader(Gl::kVertexShader, pVertexSource, mError);
    if (tVertex == 0)
    {
        return false;
    }
    const GLuint tFragment = compileShader(Gl::kFragmentShader, pFragmentSource, mError);
    if (tFragment == 0)
    {
        Gl::deleteShader(tVertex);
        return false;
    }
    const GLuint tProgram = Gl::createProgram();
    Gl::attachShader(tProgram, tVertex);
    Gl::attachShader(tProgram, tFragment);
    Gl::linkProgram(tProgram);
    Gl::deleteShader(tVertex);  // programa baglandiktan sonra ayrica gerekmez
    Gl::deleteShader(tFragment);
    if (Gl::getProgramInt(tProgram, Gl::kLinkStatus) == 0)
    {
        mError = "shader link failed: " + Gl::getProgramLog(tProgram);
        Gl::deleteProgram(tProgram);
        return false;
    }
    pOut.mId = tProgram;
    pOut.mMvpLocation = Gl::getUniformLocation(tProgram, "uMvp");
    pOut.mTransformLocation = Gl::getUniformLocation(tProgram, "uTransform");
    return true;
}

void Canvas::release()
{
    if (!Gl::isLoaded())
    {
        return;
    }
    if (mColorBuffer != 0)
    {
        Gl::deleteBuffers(1, &mColorBuffer);
        Gl::deleteBuffers(1, &mTextureBuffer);
        Gl::deleteBuffers(1, &mTextBuffer);
        mColorBuffer = 0;
        mTextureBuffer = 0;
        mTextBuffer = 0;
    }
    mColorArray.release();
    mTextureArray.release();
    mTextArray.release();
    if (mTextProgram.mId != 0)
    {
        Gl::deleteProgram(mTextProgram.mId);
        mTextProgram.mId = 0;
    }
    if (mColorProgram.mId != 0)
    {
        Gl::deleteProgram(mColorProgram.mId);
        mColorProgram.mId = 0;
    }
    if (mTextureProgram.mId != 0)
    {
        Gl::deleteProgram(mTextureProgram.mId);
        mTextureProgram.mId = 0;
    }
    mReady = false;
}

void Canvas::beginFrame()
{
    // X-Plane, boxel -> ekran matrislerini pencereye gore kurar (pop-out ve
    // UI olcek dahil); ikisinin carpimi shader'a gider.
    float tModelview[16];
    float tProjection[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, tModelview);
    glGetFloatv(GL_PROJECTION_MATRIX, tProjection);
    multiply(tProjection, tModelview, mMvp);

    // Pop-out penceresi ayri bir openGL baglamidir: degisimi not et (VAO'lar
    // yeniden olusturulur) ve shader'larin bu baglamda gecerli oldugunu dogrula
    mContextUsable = false;
    if (mReady)
    {
        Gl::noteContext();
        mContextUsable = Gl::isProgram(mColorProgram.mId) == GL_TRUE &&
                         Gl::isProgram(mTextureProgram.mId) == GL_TRUE;
    }
}

bool Canvas::isContextUsable() const
{
    return mContextUsable;
}

bool Canvas::canDraw() const
{
    return mReady && mContextUsable;
}

void Canvas::bindColorArray()
{
    mColorArray.get(
        [this]()
        {
            Gl::bindBuffer(Gl::kArrayBuffer, mColorBuffer);
            Gl::enableVertexAttribArray(0);
            Gl::vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), offsetof(ColorVertex, mX));
            Gl::enableVertexAttribArray(1);
            Gl::vertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ColorVertex),
                                    offsetof(ColorVertex, mColor));
        });
}

void Canvas::bindTextureArray()
{
    mTextureArray.get(
        [this]()
        {
            Gl::bindBuffer(Gl::kArrayBuffer, mTextureBuffer);
            Gl::enableVertexAttribArray(0);
            Gl::vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TexturedVertex), offsetof(TexturedVertex, mX));
            Gl::enableVertexAttribArray(1);
            Gl::vertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TexturedVertex), offsetof(TexturedVertex, mU));
        });
}

bool Canvas::beginOffscreen(renderTarget& pTarget, const RectD& pBoxels)
{
    if (!canDraw() || mOffscreen || pBoxels.width() <= 0.0 || pBoxels.height() <= 0.0)
    {
        return false;
    }
    flushText();  // ekrana ait bekleyen yazilar dokuya karismasin
    GLint tDraw = 0;
    GLint tRead = 0;
    glGetIntegerv(Gl::kDrawFramebufferBinding, &tDraw);
    glGetIntegerv(Gl::kReadFramebufferBinding, &tRead);
    const unsigned int tFramebuffer = pTarget.prepare(pTarget.width(), pTarget.height());
    if (tFramebuffer == 0)
    {
        return false;
    }
    mSavedDrawFramebuffer = tDraw;
    mSavedReadFramebuffer = tRead;
    glGetIntegerv(GL_VIEWPORT, mSavedViewport);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, mSavedClearColor);
    mSavedScissor = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
    for (int tIndex = 0; tIndex < 16; ++tIndex)
    {
        mSavedMvp[tIndex] = mMvp[tIndex];
    }

    Gl::bindFramebuffer(Gl::kFramebuffer, tFramebuffer);
    glViewport(0, 0, pTarget.width(), pTarget.height());
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Ortografik: pBoxels -> [-1, 1] (sutun oncelikli)
    const float tLeft = static_cast<float>(pBoxels.mLeft);
    const float tRight = static_cast<float>(pBoxels.mRight);
    const float tBottom = static_cast<float>(pBoxels.mBottom);
    const float tTop = static_cast<float>(pBoxels.mTop);
    const float tOrtho[16] = {2.0f / (tRight - tLeft), 0.0f, 0.0f, 0.0f,
                              0.0f, 2.0f / (tTop - tBottom), 0.0f, 0.0f,
                              0.0f, 0.0f, -1.0f, 0.0f,
                              -(tRight + tLeft) / (tRight - tLeft), -(tTop + tBottom) / (tTop - tBottom), 0.0f, 1.0f};
    for (int tIndex = 0; tIndex < 16; ++tIndex)
    {
        mMvp[tIndex] = tOrtho[tIndex];
    }
    mOffscreen = true;
    return true;
}

void Canvas::endOffscreen()
{
    if (!mOffscreen)
    {
        return;
    }
    flushText();  // dokuya ait yazilar dokuya
    Gl::bindFramebuffer(Gl::kDrawFramebuffer, static_cast<GLuint>(mSavedDrawFramebuffer));
    Gl::bindFramebuffer(Gl::kReadFramebuffer, static_cast<GLuint>(mSavedReadFramebuffer));
    glViewport(mSavedViewport[0], mSavedViewport[1], mSavedViewport[2], mSavedViewport[3]);
    glClearColor(mSavedClearColor[0], mSavedClearColor[1], mSavedClearColor[2], mSavedClearColor[3]);
    if (mSavedScissor)
    {
        glEnable(GL_SCISSOR_TEST);
    }
    for (int tIndex = 0; tIndex < 16; ++tIndex)
    {
        mMvp[tIndex] = mSavedMvp[tIndex];
    }
    mOffscreen = false;
}

void Canvas::bindTextArray()
{
    mTextArray.get(
        [this]()
        {
            Gl::bindBuffer(Gl::kArrayBuffer, mTextBuffer);
            Gl::enableVertexAttribArray(0);
            Gl::vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex), offsetof(TextVertex, mX));
            Gl::enableVertexAttribArray(1);
            Gl::vertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex), offsetof(TextVertex, mU));
            Gl::enableVertexAttribArray(2);
            Gl::vertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(TextVertex), offsetof(TextVertex, mColor));
        });
}

void Canvas::queueText(const Texture& pAtlas, const std::vector<TextVertex>& pVertices)
{
    if (!pAtlas.isValid() || pVertices.empty())
    {
        return;
    }
    if (mPendingAtlas != pAtlas.id())
    {
        flushText();
        mPendingAtlas = pAtlas.id();
    }
    mPendingText.insert(mPendingText.end(), pVertices.begin(), pVertices.end());
}

void Canvas::flushText()
{
    if (mPendingText.empty())
    {
        return;
    }
    if (canDraw())
    {
        XPLMSetGraphicsState(0, 1, 0, 0, 1, 0, 0);  // 1 doku birimi, saydamlik var
        XPLMBindTexture2d(mPendingAtlas, 0);

        BindingGuard tGuard;
        Gl::useProgram(mTextProgram.mId);
        Gl::uniformMatrix4(mTextProgram.mMvpLocation, mMvp);
        bindTextArray();
        Gl::bindBuffer(Gl::kArrayBuffer, mTextBuffer);
        Gl::bufferData(Gl::kArrayBuffer, static_cast<GLsizeiptr>(mPendingText.size() * sizeof(TextVertex)),
                       mPendingText.data(), Gl::kStreamDraw);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mPendingText.size()));
    }
    mPendingText.clear();
}

void Canvas::draw(const Mesh& pMesh)
{
    flushText();  // once bekleyen yazilar: ust uste binme sirasi korunsun
    if (!canDraw() || pMesh.isEmpty())
    {
        return;
    }
    const std::vector<ColorVertex>& tVertices = pMesh.vertices();
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);  // doku yok, saydamlik var, derinlik yok

    BindingGuard tGuard;
    Gl::useProgram(mColorProgram.mId);
    Gl::uniformMatrix4(mColorProgram.mMvpLocation, mMvp);
    Gl::uniform4(mColorProgram.mTransformLocation, 1.0f, 1.0f, 0.0f, 0.0f);
    bindColorArray();
    Gl::bindBuffer(Gl::kArrayBuffer, mColorBuffer);
    // Her cizimde buffer bastan ayrilir ("orphaning"): surucu eski veriyi
    // kullanan cizimi beklemeden yeni alan verir
    Gl::bufferData(Gl::kArrayBuffer, static_cast<GLsizeiptr>(tVertices.size() * sizeof(ColorVertex)),
                   tVertices.data(), Gl::kStreamDraw);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(tVertices.size()));
}

void Canvas::draw(const OutlinedLines& pLines)
{
    draw(pLines.outline());
    draw(pLines.fill());
}

void Canvas::drawStatic(const StaticMesh& pMesh, const PointF& pOffset, double pScale, float pLineWidth)
{
    flushText();
    if (!canDraw() || pMesh.isEmpty())
    {
        return;
    }
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);

    BindingGuard tGuard;
    Gl::useProgram(mColorProgram.mId);
    Gl::uniformMatrix4(mColorProgram.mMvpLocation, mMvp);
    const float tScale = static_cast<float>(pScale);
    Gl::uniform4(mColorProgram.mTransformLocation, tScale, tScale, pOffset.mX, pOffset.mY);
    pMesh.bindVertexArray();
    if (pMesh.getPrimitive() != Primitive::Lines)
    {
        glDrawArrays(GL_TRIANGLES, 0, pMesh.vertexCount());
        return;
    }
    // Kalin GL_LINES: X-Plane 11 uyumluluk (compatibility) baglaminda calisir.
    // X-Plane'in degerini bozmamak icin eskisini okuyup geri koyariz.
    GLfloat tOldWidth = 1.0f;
    const bool tWide = pLineWidth > 1.0f;
    if (tWide)
    {
        glGetFloatv(GL_LINE_WIDTH, &tOldWidth);
        glLineWidth(pLineWidth);
    }
    glDrawArrays(GL_LINES, 0, pMesh.vertexCount());
    if (tWide)
    {
        glLineWidth(tOldWidth);
    }
}

void Canvas::drawTextured(const Texture& pTexture, const std::vector<TexturedVertex>& pVertices, bool pBlend)
{
    flushText();
    if (!canDraw() || pVertices.empty() || !pTexture.isValid())
    {
        return;
    }
    XPLMSetGraphicsState(0, 1, 0, 0, pBlend ? 1 : 0, 0, 0);  // 1 doku birimi, saydamlik istege bagli
    XPLMBindTexture2d(pTexture.id(), 0);

    BindingGuard tGuard;
    Gl::useProgram(mTextureProgram.mId);
    Gl::uniformMatrix4(mTextureProgram.mMvpLocation, mMvp);
    bindTextureArray();
    Gl::bindBuffer(Gl::kArrayBuffer, mTextureBuffer);
    Gl::bufferData(Gl::kArrayBuffer, static_cast<GLsizeiptr>(pVertices.size() * sizeof(TexturedVertex)),
                   pVertices.data(), Gl::kStreamDraw);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(pVertices.size()));
}

}
