// Texture.cpp
#include "gfx/Texture.h"

#include "XPLMGraphics.h"

#include "gfx/GlApi.h"

namespace Gfx
{

Texture::Texture(int pId)
    : mId(pId)
{
}

Texture::~Texture()
{
    Release();
}

Texture::Texture(Texture&& pOther) noexcept
    : mId(pOther.mId)
{
    pOther.mId = 0;
}

Texture& Texture::operator=(Texture&& pOther) noexcept
{
    if (this != &pOther)
    {
        Release();
        mId = pOther.mId;
        pOther.mId = 0;
    }
    return *this;
}

Texture Texture::FromRgba(int pSize, const uint8_t* pRgba, bool pMipmaps)
{
    int tId = 0;
    XPLMGenerateTextureNumbers(&tId, 1);
    XPLMBindTexture2d(tId, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pSize, pSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, pRgba);
    if (pMipmaps && Gl::IsLoaded())
    {
        Gl::GenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(Gl::kLinearMipmapLinear));
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, Gl::kTextureMaxLevel, 0);  // mipmap yok: tek seviye tam sayilsin
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, Gl::kClampToEdge);  // komsu karoyla dikis olmasin
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, Gl::kClampToEdge);
    return Texture(tId);
}

bool Texture::IsValid() const
{
    return mId != 0;
}

int Texture::Id() const
{
    return mId;
}

void Texture::Release()
{
    if (mId != 0)
    {
        const GLuint tId = static_cast<GLuint>(mId);
        glDeleteTextures(1, &tId);
        mId = 0;
    }
}

}  // namespace Gfx
