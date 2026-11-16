#include "gfx/ContextVertexArray.h"

#include "gfx/GlApi.h"

namespace Gfx
{

ContextVertexArray::~ContextVertexArray()
{
    release();
}

ContextVertexArray::ContextVertexArray(ContextVertexArray&& pOther) noexcept
    : mContext(pOther.mContext)
    , mGeneration(pOther.mGeneration)
    , mArray(pOther.mArray)
{
    pOther.mContext = nullptr;
    pOther.mArray = 0;
}

ContextVertexArray& ContextVertexArray::operator=(ContextVertexArray&& pOther) noexcept
{
    if (this != &pOther)
    {
        release();
        mContext = pOther.mContext;
        mGeneration = pOther.mGeneration;
        mArray = pOther.mArray;
        pOther.mContext = nullptr;
        pOther.mArray = 0;
    }
    return *this;
}

unsigned int ContextVertexArray::get(const std::function<void()>& pSetup)
{
    void* tContext = Gl::currentContext();
    if (mArray != 0 && mContext == tContext && mGeneration == Gl::contextGeneration())
    {
        Gl::bindVertexArray(mArray);
        return mArray;  // ayni baglam, arada baska baglama gecilmedi: gecerli
    }
    // Baglam degisti (pop-out / pop-in) ya da hic olusturulmadi
    release();
    Gl::genVertexArrays(1, &mArray);
    Gl::bindVertexArray(mArray);
    pSetup();
    mContext = tContext;
    mGeneration = Gl::contextGeneration();
    return mArray;
}

void ContextVertexArray::release()
{
    // Sadece olusturuldugu baglam hala etkinse ve arada baglam degismediyse
    // silinir. Nesil degistiyse ayni kimlik yeni (baska) bir baglama ait
    // olabilir; o adi silmek X-Plane'in kendi VAO'sunu silebilir. O durumda
    // sadece unutulur (pop-out basina birkac kucuk ad bosta kalir, zararsiz).
    if (mArray != 0 && Gl::isLoaded() && mContext == Gl::currentContext() &&
        mGeneration == Gl::contextGeneration())
    {
        Gl::deleteVertexArrays(1, &mArray);
    }
    mArray = 0;
    mContext = nullptr;
}

}
