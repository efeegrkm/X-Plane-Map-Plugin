// GL baglamina ozel vertex array tutucusu; baglam degisince yeniden olusturulur.
#ifndef PRIME_MAP_SRC_GFX_CONTEXTVERTEXARRAY_H
#define PRIME_MAP_SRC_GFX_CONTEXTVERTEXARRAY_H

#include <functional>

namespace Gfx
{
class ContextVertexArray
{
public:
    ContextVertexArray() = default;
    ~ContextVertexArray();

    ContextVertexArray(ContextVertexArray&& pOther) noexcept;
    ContextVertexArray& operator=(ContextVertexArray&& pOther) noexcept;
    ContextVertexArray(const ContextVertexArray&) = delete;
    ContextVertexArray& operator=(const ContextVertexArray&) = delete;

    unsigned int get(const std::function<void()>& pSetup);

    void release();

private:
    void* mContext = nullptr;
    unsigned int mGeneration = 0;
    unsigned int mArray = 0;
};
}
#endif
