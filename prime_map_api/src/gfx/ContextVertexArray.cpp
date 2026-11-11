// ContextVertexArray.cpp
#include "gfx/ContextVertexArray.h"

#include "gfx/GlApi.h"

namespace Gfx
{

ContextVertexArray::~ContextVertexArray()
{
    Release();
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
        Release();
        mContext = pOther.mContext;
        mGeneration = pOther.mGeneration;
        mArray = pOther.mArray;
        pOther.mContext = nullptr;
        pOther.mArray = 0;
    }
    return *this;
}

unsigned int ContextVertexArray::Get(const std::function<void()>& pSetup)
{
    void* tContext = Gl::CurrentContext();
    if (mArray != 0 && mContext == tContext && mGeneration == Gl::ContextGeneration())
    {
        Gl::BindVertexArray(mArray);
        return mArray;  // ayni baglam, arada baska baglama gecilmedi: gecerli
    }
    // Baglam degisti (pop-out / pop-in) ya da hic olusturulmadi
    Release();
    Gl::GenVertexArrays(1, &mArray);
    Gl::BindVertexArray(mArray);
    pSetup();
    mContext = tContext;
    mGeneration = Gl::ContextGeneration();
    return mArray;
}

void ContextVertexArray::Release()
{
    // Sadece olusturuldugu baglam hala etkinse ve arada baglam degismediyse
    // silinir. Nesil degistiyse ayni kimlik yeni (baska) bir baglama ait
    // olabilir; o adi silmek X-Plane'in kendi VAO'sunu silebilir. O durumda
    // sadece unutulur (pop-out basina birkac kucuk ad bosta kalir, zararsiz).
    if (mArray != 0 && Gl::IsLoaded() && mContext == Gl::CurrentContext() &&
        mGeneration == Gl::ContextGeneration())
    {
        Gl::DeleteVertexArrays(1, &mArray);
    }
    mArray = 0;
    mContext = nullptr;
}

}  // namespace Gfx
