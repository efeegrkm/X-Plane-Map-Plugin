// RenderTarget.cpp
#include "gfx/RenderTarget.h"

#include "gfx/GlApi.h"

namespace Gfx
{

RenderTarget::~RenderTarget()
{
    ReleaseFramebuffer();
}

unsigned int RenderTarget::Prepare(int pWidth, int pHeight)
{
    if (pWidth <= 0 || pHeight <= 0)
    {
        return 0;
    }
    const bool tResized = !mTexture.IsValid() || pWidth != mWidth || pHeight != mHeight;
    if (tResized)
    {
        ReleaseFramebuffer();  // eski dokuya bagli FBO artik gecersiz
        mTexture = Texture::CreateEmpty(pWidth, pHeight);
        mWidth = pWidth;
        mHeight = pHeight;
    }
    void* tContext = Gl::CurrentContext();
    if (mFramebuffer != 0 && mContext == tContext && mGeneration == Gl::ContextGeneration())
    {
        return mFramebuffer;
    }
    // Ilk kez ya da baska bir baglam (pop-out / pop-in): bu baglam icin yeni FBO
    ReleaseFramebuffer();
    GLint tPreviousDraw = 0;
    GLint tPreviousRead = 0;
    glGetIntegerv(Gl::kDrawFramebufferBinding, &tPreviousDraw);
    glGetIntegerv(Gl::kReadFramebufferBinding, &tPreviousRead);
    GLuint tFramebuffer = 0;
    Gl::GenFramebuffers(1, &tFramebuffer);
    Gl::BindFramebuffer(Gl::kFramebuffer, tFramebuffer);
    Gl::FramebufferTexture2D(Gl::kFramebuffer, Gl::kColorAttachment0, GL_TEXTURE_2D,
                             static_cast<GLuint>(mTexture.Id()), 0);
    const bool tComplete = Gl::CheckFramebufferStatus(Gl::kFramebuffer) == Gl::kFramebufferComplete;
    Gl::BindFramebuffer(Gl::kDrawFramebuffer, static_cast<GLuint>(tPreviousDraw));
    Gl::BindFramebuffer(Gl::kReadFramebuffer, static_cast<GLuint>(tPreviousRead));
    if (!tComplete)
    {
        Gl::DeleteFramebuffers(1, &tFramebuffer);
        return 0;
    }
    mFramebuffer = tFramebuffer;
    mContext = tContext;
    mGeneration = Gl::ContextGeneration();
    return mFramebuffer;
}

const Texture& RenderTarget::GetTexture() const
{
    return mTexture;
}

int RenderTarget::Width() const
{
    return mWidth;
}

int RenderTarget::Height() const
{
    return mHeight;
}

void RenderTarget::ReleaseFramebuffer()
{
    // Sadece olusturuldugu baglam hala etkinse silinir (ContextVertexArray ile ayni kural);
    // aksi halde ayni ad baska baglamda X-Plane'in kendi nesnesi olabilir: sadece unutulur.
    if (mFramebuffer != 0 && Gl::IsLoaded() && mContext == Gl::CurrentContext() &&
        mGeneration == Gl::ContextGeneration())
    {
        Gl::DeleteFramebuffers(1, &mFramebuffer);
    }
    mFramebuffer = 0;
    mContext = nullptr;
}

}  // namespace Gfx
