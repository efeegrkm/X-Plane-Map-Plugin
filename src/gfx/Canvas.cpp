// Canvas.cpp
#include "gfx/Canvas.h"

#include <cstddef>

#include "XPLMGraphics.h"

#include "gfx/GlApi.h"
#include "gfx/StaticMesh.h"
#include "gfx/Texture.h"

namespace
{

// --- Shader'lar (GLSL 3.30) ---
// Renkli: her kosenin kendi rengi var. uTransform kosesi once boxel'e
// tasir (GPU'da duran sekiller icin: olcek + kaydirma; normal cizimde 1, 0),
// matris de boxel'den ekrana.
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

// Dokulu: renk dokudan (0. doku birimi)
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

// Yazi: renk koseden, alfa = kose alfasi x atlas kaplamasi
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

// X-Plane'in bagli tuttugu nesneler: cizimden once kaydedilip sonra geri konur
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
        Gl::BindVertexArray(static_cast<GLuint>(mArray));
        Gl::BindBuffer(Gl::kArrayBuffer, static_cast<GLuint>(mBuffer));
        Gl::UseProgram(static_cast<GLuint>(mProgram));
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

// Sutun oncelikli 4x4 matris carpimi: pOut = pLeft * pRight
void Multiply(const float pLeft[16], const float pRight[16], float pOut[16])
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

GLuint CompileShader(GLenum pType, const char* pSource, std::string& pError)
{
    const GLuint tShader = Gl::CreateShader(pType);
    Gl::ShaderSource(tShader, pSource);
    Gl::CompileShader(tShader);
    if (Gl::GetShaderInt(tShader, Gl::kCompileStatus) == 0)
    {
        pError = "shader compile failed: " + Gl::GetShaderLog(tShader);
        Gl::DeleteShader(tShader);
        return 0;
    }
    return tShader;
}

}  // namespace

namespace Gfx
{

Canvas::~Canvas()
{
    Release();
}

bool Canvas::Init()
{
    if (mTried)
    {
        return mReady;
    }
    mTried = true;
    if (!Gl::Load(mError))
    {
        return false;
    }
    mContext = Gl::DescribeContext();
    if (!BuildProgram(kColorVertexShader, kColorFragmentShader, mColorProgram) ||
        !BuildProgram(kTextureVertexShader, kTextureFragmentShader, mTextureProgram) ||
        !BuildProgram(kTextVertexShader, kTextFragmentShader, mTextProgram))
    {
        return false;
    }

    // Vertex buffer'lar burada (baglamlar arasi paylasilir). VAO'lar ise
    // ilk cizimde, o anki baglam icin olusturulur (BindColorArray).
    Gl::GenBuffers(1, &mColorBuffer);
    Gl::GenBuffers(1, &mTextureBuffer);
    Gl::GenBuffers(1, &mTextBuffer);

    BindingGuard tGuard;
    Gl::UseProgram(mTextureProgram.mId);
    Gl::Uniform1i(Gl::GetUniformLocation(mTextureProgram.mId, "uTexture"), 0);
    Gl::UseProgram(mTextProgram.mId);
    Gl::Uniform1i(Gl::GetUniformLocation(mTextProgram.mId, "uTexture"), 0);

    mReady = true;
    return true;
}

bool Canvas::IsReady() const
{
    return mReady;
}

const std::string& Canvas::ErrorText() const
{
    return mError;
}

const std::string& Canvas::ContextText() const
{
    return mContext;
}

bool Canvas::BuildProgram(const char* pVertexSource, const char* pFragmentSource, Program& pOut)
{
    const GLuint tVertex = CompileShader(Gl::kVertexShader, pVertexSource, mError);
    if (tVertex == 0)
    {
        return false;
    }
    const GLuint tFragment = CompileShader(Gl::kFragmentShader, pFragmentSource, mError);
    if (tFragment == 0)
    {
        Gl::DeleteShader(tVertex);
        return false;
    }
    const GLuint tProgram = Gl::CreateProgram();
    Gl::AttachShader(tProgram, tVertex);
    Gl::AttachShader(tProgram, tFragment);
    Gl::LinkProgram(tProgram);
    Gl::DeleteShader(tVertex);  // programa baglandiktan sonra ayrica gerekmez
    Gl::DeleteShader(tFragment);
    if (Gl::GetProgramInt(tProgram, Gl::kLinkStatus) == 0)
    {
        mError = "shader link failed: " + Gl::GetProgramLog(tProgram);
        Gl::DeleteProgram(tProgram);
        return false;
    }
    pOut.mId = tProgram;
    pOut.mMvpLocation = Gl::GetUniformLocation(tProgram, "uMvp");
    pOut.mTransformLocation = Gl::GetUniformLocation(tProgram, "uTransform");
    return true;
}

void Canvas::Release()
{
    if (!Gl::IsLoaded())
    {
        return;
    }
    if (mColorBuffer != 0)
    {
        Gl::DeleteBuffers(1, &mColorBuffer);
        Gl::DeleteBuffers(1, &mTextureBuffer);
        Gl::DeleteBuffers(1, &mTextBuffer);
        mColorBuffer = 0;
        mTextureBuffer = 0;
        mTextBuffer = 0;
    }
    mColorArray.Release();
    mTextureArray.Release();
    mTextArray.Release();
    if (mTextProgram.mId != 0)
    {
        Gl::DeleteProgram(mTextProgram.mId);
        mTextProgram.mId = 0;
    }
    if (mColorProgram.mId != 0)
    {
        Gl::DeleteProgram(mColorProgram.mId);
        mColorProgram.mId = 0;
    }
    if (mTextureProgram.mId != 0)
    {
        Gl::DeleteProgram(mTextureProgram.mId);
        mTextureProgram.mId = 0;
    }
    mReady = false;
}

void Canvas::BeginFrame()
{
    // X-Plane pencere cizerken boxel koordinatini ekrana tasiyan matrisleri
    // kurmus olur (pop-out ve UI olcek dahil). Ikisinin carpimi shader'a gider.
    float tModelview[16];
    float tProjection[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, tModelview);
    glGetFloatv(GL_PROJECTION_MATRIX, tProjection);
    Multiply(tProjection, tModelview, mMvp);

    // Pop-out penceresi ayri bir OpenGL baglamidir: degisimi not et (VAO'lar
    // yeniden olusturulur) ve shader'larimizin bu baglamda gecerli oldugunu dogrula
    mContextUsable = false;
    if (mReady)
    {
        Gl::NoteContext();
        mContextUsable = Gl::IsProgram(mColorProgram.mId) == GL_TRUE &&
                         Gl::IsProgram(mTextureProgram.mId) == GL_TRUE;
    }
}

bool Canvas::IsContextUsable() const
{
    return mContextUsable;
}

bool Canvas::CanDraw() const
{
    return mReady && mContextUsable;
}

void Canvas::BindColorArray()
{
    mColorArray.Get(
        [this]()
        {
            // Renkli koseler: x, y (2 float) + renk (4 float)
            Gl::BindBuffer(Gl::kArrayBuffer, mColorBuffer);
            Gl::EnableVertexAttribArray(0);
            Gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), offsetof(ColorVertex, mX));
            Gl::EnableVertexAttribArray(1);
            Gl::VertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ColorVertex),
                                    offsetof(ColorVertex, mColor));
        });
}

void Canvas::BindTextureArray()
{
    mTextureArray.Get(
        [this]()
        {
            // Dokulu koseler: x, y + u, v
            Gl::BindBuffer(Gl::kArrayBuffer, mTextureBuffer);
            Gl::EnableVertexAttribArray(0);
            Gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TexturedVertex), offsetof(TexturedVertex, mX));
            Gl::EnableVertexAttribArray(1);
            Gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TexturedVertex), offsetof(TexturedVertex, mU));
        });
}

void Canvas::BindTextArray()
{
    mTextArray.Get(
        [this]()
        {
            // Yazi koseleri: x, y + u, v + renk
            Gl::BindBuffer(Gl::kArrayBuffer, mTextBuffer);
            Gl::EnableVertexAttribArray(0);
            Gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex), offsetof(TextVertex, mX));
            Gl::EnableVertexAttribArray(1);
            Gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex), offsetof(TextVertex, mU));
            Gl::EnableVertexAttribArray(2);
            Gl::VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(TextVertex), offsetof(TextVertex, mColor));
        });
}

void Canvas::QueueText(const Texture& pAtlas, const std::vector<TextVertex>& pVertices)
{
    if (!pAtlas.IsValid() || pVertices.empty())
    {
        return;
    }
    if (mPendingAtlas != pAtlas.Id())
    {
        FlushText();
        mPendingAtlas = pAtlas.Id();
    }
    mPendingText.insert(mPendingText.end(), pVertices.begin(), pVertices.end());
}

void Canvas::FlushText()
{
    if (mPendingText.empty())
    {
        return;
    }
    if (CanDraw())
    {
        XPLMSetGraphicsState(0, 1, 0, 0, 1, 0, 0);  // 1 doku birimi, saydamlik var
        XPLMBindTexture2d(mPendingAtlas, 0);

        BindingGuard tGuard;
        Gl::UseProgram(mTextProgram.mId);
        Gl::UniformMatrix4(mTextProgram.mMvpLocation, mMvp);
        BindTextArray();
        Gl::BindBuffer(Gl::kArrayBuffer, mTextBuffer);
        Gl::BufferData(Gl::kArrayBuffer, static_cast<GLsizeiptr>(mPendingText.size() * sizeof(TextVertex)),
                       mPendingText.data(), Gl::kStreamDraw);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mPendingText.size()));
    }
    mPendingText.clear();
}

void Canvas::Draw(const Mesh& pMesh)
{
    FlushText();  // once bekleyen yazilar: ust uste binme sirasi korunsun
    if (!CanDraw() || pMesh.IsEmpty())
    {
        return;
    }
    const std::vector<ColorVertex>& tVertices = pMesh.Vertices();
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);  // doku yok, saydamlik var, derinlik yok

    BindingGuard tGuard;
    Gl::UseProgram(mColorProgram.mId);
    Gl::UniformMatrix4(mColorProgram.mMvpLocation, mMvp);
    Gl::Uniform4(mColorProgram.mTransformLocation, 1.0f, 1.0f, 0.0f, 0.0f);
    BindColorArray();
    Gl::BindBuffer(Gl::kArrayBuffer, mColorBuffer);
    // Her cizimde buffer bastan ayrilir ("orphaning"): surucu eski veriyi
    // kullanan cizimi beklemeden yeni alan verir
    Gl::BufferData(Gl::kArrayBuffer, static_cast<GLsizeiptr>(tVertices.size() * sizeof(ColorVertex)),
                   tVertices.data(), Gl::kStreamDraw);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(tVertices.size()));
}

void Canvas::Draw(const OutlinedLines& pLines)
{
    Draw(pLines.Outline());
    Draw(pLines.Fill());
}

void Canvas::DrawStatic(const StaticMesh& pMesh, const PointF& pOffset, double pScale, float pLineWidth)
{
    FlushText();
    if (!CanDraw() || pMesh.IsEmpty())
    {
        return;
    }
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);

    BindingGuard tGuard;
    Gl::UseProgram(mColorProgram.mId);
    Gl::UniformMatrix4(mColorProgram.mMvpLocation, mMvp);
    const float tScale = static_cast<float>(pScale);
    Gl::Uniform4(mColorProgram.mTransformLocation, tScale, tScale, pOffset.mX, pOffset.mY);
    pMesh.BindVertexArray();  // bu baglam icin (gerekirse olusturur)
    if (pMesh.GetPrimitive() != Primitive::Lines)
    {
        glDrawArrays(GL_TRIANGLES, 0, pMesh.VertexCount());
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
    glDrawArrays(GL_LINES, 0, pMesh.VertexCount());
    if (tWide)
    {
        glLineWidth(tOldWidth);
    }
}

void Canvas::DrawTextured(const Texture& pTexture, const std::vector<TexturedVertex>& pVertices)
{
    FlushText();
    if (!CanDraw() || pVertices.empty() || !pTexture.IsValid())
    {
        return;
    }
    XPLMSetGraphicsState(0, 1, 0, 0, 1, 0, 0);  // 1 doku birimi, saydamlik var
    XPLMBindTexture2d(pTexture.Id(), 0);

    BindingGuard tGuard;
    Gl::UseProgram(mTextureProgram.mId);
    Gl::UniformMatrix4(mTextureProgram.mMvpLocation, mMvp);
    BindTextureArray();
    Gl::BindBuffer(Gl::kArrayBuffer, mTextureBuffer);
    Gl::BufferData(Gl::kArrayBuffer, static_cast<GLsizeiptr>(pVertices.size() * sizeof(TexturedVertex)),
                   pVertices.data(), Gl::kStreamDraw);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(pVertices.size()));
}

}  // namespace Gfx
