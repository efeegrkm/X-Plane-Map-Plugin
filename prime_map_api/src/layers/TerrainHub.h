// TerrainHub.h
// Zemin verisinin haritalar arasi ORTAK kullanimi (0.23, coklu harita).
//
// Zemin iscisi (TerrainStore) surecte tektir; urettigi goruntu ve vektorleri "ilk alan"
// sahiplenseydi iki harita birbirinin sonucunu calardi. Bu sinif sonuclari tek yerde
// toplar, her haritanin TerrainLayer / RoadLayer'i buradan okur:
//
//   TerrainStore (isci) --PopImage----> TerrainHub: dokular (GPU, tek kopya)  <-- TerrainLayer (harita 1)
//                       --PopVectors--> TerrainHub: vektorler (bellek)        <-- TerrainLayer (harita 2)
//                                                                             <-- RoadLayer    (harita 1, 2)
//
// Kazanc: iki harita ayni yere bakiyorsa doku bir kez uretilir, bir kez yuklenir, video
// belleginde bir kez durur. Doku butcesi tek ve ortaktir; acik harita sayisina gore
// buyutulur (MapData::ApplyLimits, prime_map.ini [memory]).
//
// Kullanici (user): her harita bir numara alir (AddUser). Bir doku / vektor, onu son
// ciziminde kullanan haritalarin bitleriyle isaretlidir; isaretli olan atilmaz. Harita
// her cizimin basinda kendi isaretlerini siler (BeginDraw), cizerken yeniden koyar (Use...).
//
// Sadece ana thread. Doku yukleme / silme OpenGL baglami etkinken (pencere cizimi) yapilir.
// OpenGL cagrisi yok: dokular gfx/Texture uzerinden.
#pragma once

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

    TerrainStore& Store();

    // --- Kullanicilar (haritalar) ---
    int AddUser();                // bu haritanin numarasi (0..31; 32. haritadan sonra son numara paylasilir)
    void RemoveUser(int pUser);   // harita kapandi: isaretleri silinir (dokular butceye gore atilir)

    // --- Dokular ---
    // Flight loop'tan her karede bir kez: karedeki doku yukleme hakkini yeniler (FPS korumasi)
    void BeginFrame();
    // Hazir goruntuleri dokuya yukler (karede en cok kUploadsPerFrame; OpenGL baglami etkin olmali).
    // Yuklediyse true.
    bool UploadReady();
    // Her yuklemede artar: katman "son gordugumden beri yeni doku geldi mi" diye bakar
    uint64_t TextureGeneration() const;
    // Haritanin Draw'i baslarken: bu haritanin doku isaretleri silinir
    void BeginDraw(int pUser);
    // Varsa dokuyu verir ve bu harita icin isaretler; yoksa nullptr. Eksik komsularindan biri
    // artik hazirsa dokuyu yeniden ister (eskisi yenisi gelene kadar kullanilir).
    const Gfx::Texture* UseTexture(const TerrainImageKey& pKey, int pUser);
    // Butce asildiysa hicbir haritanin isaretlemedigi dokulardan en eskiler silinir
    void EvictUnusedTextures();
    void SetTextureBudget(size_t pBytes);
    size_t TextureBudget() const;
    size_t TextureBytes() const;
    int TextureCount() const;

    // --- Vektorler (yol katmani) ---
    // Hazir vektorleri isciden alir; aldiysa true
    bool TakeReadyVectors();
    uint64_t VectorGeneration() const;
    // Karonun vektorleri: varsa verir ve bu harita icin isaretler; yoksa isciden ister, nullptr doner
    std::shared_ptr<const TerrainVectors> UseVectors(int pWest, int pSouth, int pUser);
    // Harita bu karonun vektorlerini artik tutmuyor; kimse tutmuyorsa bellekten atilir
    void ReleaseVectors(int pWest, int pSouth, int pUser);

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

    static uint32_t BitOf(int pUser);
    void DropUnusedVectors();

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
