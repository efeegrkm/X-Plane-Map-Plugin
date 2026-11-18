// Zemin dokularini ve yol vektorlerini bir yoneticinin haritalari arasinda paylastirir.
#ifndef PRIME_MAP_SRC_LAYERS_TERRAINHUB_H
#define PRIME_MAP_SRC_LAYERS_TERRAINHUB_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "data/TerrainStore.h"
#include "gfx/Texture.h"

class TerrainVectors;

class TerrainHub
{
public:
    static const size_t kDefaultTextureBudget = 512u << 20;

    explicit TerrainHub(TerrainStore& pStore);

    TerrainHub(const TerrainHub&) = delete;
    TerrainHub& operator=(const TerrainHub&) = delete;

    TerrainStore& store();

    int addUser();
    void removeUser(int pUser);

    void beginFrame();
    bool uploadReady();
    uint64_t textureGeneration() const;
    void beginDraw(int pUser);
    const Gfx::Texture* useTexture(const TerrainImageKey& pKey, int pUser);
    void evictUnusedTextures();
    void setTextureBudget(size_t pBytes);
    size_t textureBudget() const;
    size_t textureBytes() const;
    int textureCount() const;

    bool takeReadyVectors();
    uint64_t vectorGeneration() const;
    std::shared_ptr<const TerrainVectors> useVectors(int pWest, int pSouth, int pUser);
    void releaseVectors(int pWest, int pSouth, int pUser);

private:
    struct CachedTexture
    {
        Gfx::Texture mTexture;
        size_t mBytes = 0;
        uint64_t mLastUse = 0;
        uint32_t mUsers = 0;
        std::vector<int> mMissingNeighbors;
    };

    struct CachedVectors
    {
        std::shared_ptr<const TerrainVectors> mVectors;
        uint32_t mUsers = 0;
    };

    static uint32_t bitOf(int pUser);
    void dropUnusedVectors();

    TerrainStore& mStore;
    uint32_t mUsedUsers = 0;

    std::unordered_map<TerrainImageKey, CachedTexture, TerrainImageKeyHash> mTextures;
    size_t mTextureBytes = 0;
    size_t mTextureBudget = kDefaultTextureBudget;
    uint64_t mTextureGeneration = 0;
    uint64_t mUseClock = 0;
    int mUploadsLeft = 0;

    std::unordered_map<int, CachedVectors> mVectors;
    uint64_t mVectorGeneration = 0;
};
#endif
