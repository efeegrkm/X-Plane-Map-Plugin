#include "gfx/renderTarget.h"

#include "gfx/GlApi.h"

namespace Gfx
{

renderTarget::~renderTarget()
{
    releaseFramebuffer();
}

unsigned int renderTarget::prepare(int pWidth, int pHeight)
{
    if (pWidth <= 0 || pHeight <= 0)
    {
        return 0;
    }
    const bool tResized = !mTexture.isValid() || pWidth != mWidth || pHeight != mHeight;
    if (tResized)
    {
        releaseFramebuffer();  // eski dokuya bagli FBO artik gecersiz
        mTexture = Texture::createEmpty(pWidth, pHeight);
        mWidth = pWidth;
        mHeight = pHeight;
    }
    void* tContext = Gl::currentContext();
    if (mFramebuffer != 0 && mContext == tContext && mGeneration == Gl::contextGeneration())
    {
        return mFramebuffer;
    }
    // Ilk kez ya da baska bir baglam (pop-out / pop-in): bu baglam icin yeni FBO
    releaseFramebuffer();
    GLint tPreviousDraw = 0;
    GLint tPreviousRead = 0;
    glGetIntegerv(Gl::kDrawFramebufferBinding, &tPreviousDraw);
    glGetIntegerv(Gl::kReadFramebufferBinding, &tPreviousRead);
    GLuint tFramebuffer = 0;
    Gl::genFramebuffers(1, &tFramebuffer);
    Gl::bindFramebuffer(Gl::kFramebuffer, tFramebuffer);
    Gl::framebufferTexture2D(Gl::kFramebuffer, Gl::kColorAttachment0, GL_TEXTURE_2D,
                             static_cast<GLuint>(mTexture.id()), 0);
    const bool tComplete = Gl::checkFramebufferStatus(Gl::kFramebuffer) == Gl::kFramebufferComplete;
    Gl::bindFramebuffer(Gl::kDrawFramebuffer, static_cast<GLuint>(tPreviousDraw));
    Gl::bindFramebuffer(Gl::kReadFramebuffer, static_cast<GLuint>(tPreviousRead));
    if (!tComplete)
    {
        Gl::deleteFramebuffers(1, &tFramebuffer);
        return 0;
    }
    mFramebuffer = tFramebuffer;
    mContext = tContext;
    mGeneration = Gl::contextGeneration();
    return mFramebuffer;
}

const Texture& renderTarget::getTexture() const
{
    return mTexture;
}

int renderTarget::width() const
{
    return mWidth;
}

int renderTarget::height() const
{
    return mHeight;
}

void renderTarget::releaseFramebuffer()
{
    // Sadece olusturuldugu baglam hala etkinse silinir (ContextVertexArray ile ayni kural);
    // aksi halde ayni ad baska baglamda X-Plane'in kendi nesnesi olabilir: sadece unutulur.
    if (mFramebuffer != 0 && Gl::isLoaded() && mContext == Gl::currentContext() &&
        mGeneration == Gl::contextGeneration())
    {
        Gl::deleteFramebuffers(1, &mFramebuffer);
    }
    mFramebuffer = 0;
    mContext = nullptr;
}

}
