// Zemin verisinin haritalar arasi ORTAK kullanimi: TerrainStore (isci) surecte
// tektir; dokular ve vektorler tek yerde toplanir, haritalar buradan okur.
// Sadece ana thread; doku yukleme/silme GL baglami etkinken yapilmali.
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
    static const size_t kDefaultTextureBudget = 512u << 20;  // tek harita icin ~512 MB video bellegi

    explicit TerrainHub(TerrainStore& pStore);

    TerrainHub(const TerrainHub&) = delete;
    TerrainHub& operator=(const TerrainHub&) = delete;

    TerrainStore& store();

    // kullanicilar: her harita bir numara alir; doku/vektor onu son ciziminde
    // kullanan haritalarin bitleriyle isaretlidir, isaretsiz olan atilir.
    int addUser();                // (0..31; 32. haritadan sonra son numara paylasilir)
    void removeUser(int pUser);   // harita kapandi

    // Flight loop'tan her karede bir kez: karedeki doku yukleme hakkini yeniler (FPS korumasi)
    void beginFrame();
    // Hazir goruntuleri dokuya yukler (karede en cok kUploadsPerFrame; GL baglami etkin olmali)
    bool uploadReady();
    // Her yuklemede artar: katman "yeni doku geldi mi" diye bakar
    uint64_t textureGeneration() const;
    void beginDraw(int pUser);
    // Varsa dokuyu verir ve bu harita icin isaretler; yoksa nullptr. Eksik komsusu
    // hazirsa doku yeniden istenir (karo sinirinda golge dikisi olmasin).
    const Gfx::Texture* useTexture(const TerrainImageKey& pKey, int pUser);
    void evictUnusedTextures();
    void setTextureBudget(size_t pBytes);
    size_t textureBudget() const;
    size_t textureBytes() const;
    int textureCount() const;

    // Hazir vektorleri isciden alir; aldiysa true
    bool takeReadyVectors();
    uint64_t vectorGeneration() const;
    std::shared_ptr<const TerrainVectors> useVectors(int pWest, int pSouth, int pUser);
    void releaseVectors(int pWest, int pSouth, int pUser);

private:
    struct CachedTexture
    {
        Gfx::Texture mTexture;
        size_t mBytes = 0;
        uint64_t mLastUse = 0;               // mUseClock degeri (en eski once atilir)
        uint32_t mUsers = 0;                 // son ciziminde kullanan haritalarin bitleri
        std::vector<int> mMissingNeighbors;  // uretilirken rakimi olmayan komsular (TerrainImage)
    };

    struct CachedVectors
    {
        std::shared_ptr<const TerrainVectors> mVectors;
        uint32_t mUsers = 0;
    };

    static uint32_t bitOf(int pUser);
    void dropUnusedVectors();

    TerrainStore& mStore;
    uint32_t mUsedUsers = 0;  // verilmis kullanici numaralari

    std::unordered_map<TerrainImageKey, CachedTexture, TerrainImageKeyHash> mTextures;
    size_t mTextureBytes = 0;
    size_t mTextureBudget = kDefaultTextureBudget;
    uint64_t mTextureGeneration = 0;
    uint64_t mUseClock = 0;
    int mUploadsLeft = 0;

    std::unordered_map<int, CachedVectors> mVectors;  // SceneryIndex::TileKey
    uint64_t mVectorGeneration = 0;
};
#endif  // PRIME_MAP_SRC_LAYERS_TERRAINHUB_H
