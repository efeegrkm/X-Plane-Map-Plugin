#include "layers/TerrainHub.h"

#include <algorithm>
#include <utility>

#include "data/SceneryIndex.h"
#include "data/TerrainVectors.h"

namespace
{

const int kUploadsPerFrame = 3;       // karede en fazla bu kadar doku yuklenir (FPS korumasi)
const int kMaxUsers = 32;             // isaretler 32 bitlik
const size_t kMaxIdleVectorTiles = 4; // kimsenin tutmadigi vektor karolari bundan cok olunca atilir

}

TerrainHub::TerrainHub(TerrainStore& pStore)
    : mStore(pStore)
    , mUploadsLeft(kUploadsPerFrame)
{
}

TerrainStore& TerrainHub::store()
{
    return mStore;
}

uint32_t TerrainHub::bitOf(int pUser)
{
    return 1u << static_cast<uint32_t>(std::clamp(pUser, 0, kMaxUsers - 1));
}

int TerrainHub::addUser()
{
    for (int tUser = 0; tUser < kMaxUsers; ++tUser)
    {
        if ((mUsedUsers & bitOf(tUser)) == 0)
        {
            mUsedUsers |= bitOf(tUser);
            return tUser;
        }
    }
    // 32'den fazla harita: son numara paylasilir (o haritalar birbirinin isaretini siler; sonuc
    // sadece gereksiz yeniden istek olur, yanlis cizim degil)
    return kMaxUsers - 1;
}

void TerrainHub::removeUser(int pUser)
{
    const uint32_t tBit = bitOf(pUser);
    mUsedUsers &= ~tBit;
    for (auto& tEntry : mTextures)
    {
        tEntry.second.mUsers &= ~tBit;
    }
    for (auto& tEntry : mVectors)
    {
        tEntry.second.mUsers &= ~tBit;
    }
    dropUnusedVectors();
}

void TerrainHub::beginFrame()
{
    mUploadsLeft = kUploadsPerFrame;
}

bool TerrainHub::uploadReady()
{
    bool tUploaded = false;
    TerrainImage tImage;
    while (mUploadsLeft > 0 && mStore.popImage(&tImage))
    {
        --mUploadsLeft;
        uint32_t tUsers = 0;
        const auto tExisting = mTextures.find(tImage.mKey);
        if (tExisting != mTextures.end())
        {
            if (tExisting->second.mMissingNeighbors.empty())
            {
                continue;  // ayni goruntu iki kez istenmis olabilir
            }
            // Komsusu eksik uretilmis eski doku: yenisi yerine gecer (isaretleri korunur)
            tUsers = tExisting->second.mUsers;
            mTextureBytes -= tExisting->second.mBytes;
            mTextures.erase(tExisting);
        }
        CachedTexture tCached;
        tCached.mTexture = Gfx::Texture::fromRgba(tImage.mSize, tImage.mRgba.data(), true);
        tCached.mBytes = tImage.mRgba.size() * 4 / 3;  // mipmap seviyeleri dahil
        tCached.mLastUse = ++mUseClock;
        tCached.mUsers = tUsers;
        tCached.mMissingNeighbors = std::move(tImage.mMissingNeighbors);
        mTextureBytes += tCached.mBytes;
        mTextures.emplace(tImage.mKey, std::move(tCached));
        tUploaded = true;
    }
    if (tUploaded)
    {
        ++mTextureGeneration;
    }
    return tUploaded;
}

uint64_t TerrainHub::textureGeneration() const
{
    return mTextureGeneration;
}

void TerrainHub::beginDraw(int pUser)
{
    const uint32_t tBit = bitOf(pUser);
    for (auto& tEntry : mTextures)
    {
        tEntry.second.mUsers &= ~tBit;
    }
}

const Gfx::Texture* TerrainHub::useTexture(const TerrainImageKey& pKey, int pUser)
{
    const auto tFound = mTextures.find(pKey);
    if (tFound == mTextures.end())
    {
        return nullptr;
    }
    tFound->second.mUsers |= bitOf(pUser);
    tFound->second.mLastUse = ++mUseClock;
    for (const int tNeighbor : tFound->second.mMissingNeighbors)
    {
        // TileKey -> karo (SceneryIndex::TileKey'in tersi)
        if (mStore.hasElevation(tNeighbor % 360 - 180, tNeighbor / 360 - 90))
        {
            mStore.Request(pKey);  // ayni anahtar uretilirken / kuyruktayken tekrar alinmaz
            break;
        }
    }
    return &tFound->second.mTexture;
}

void TerrainHub::evictUnusedTextures()
{
    // Butce asildiysa hicbir haritanin son ciziminde kullanmadigi dokulardan en eskiler silinir
    while (mTextureBytes > mTextureBudget)
    {
        auto tOldest = mTextures.end();
        for (auto tIterator = mTextures.begin(); tIterator != mTextures.end(); ++tIterator)
        {
            if (tIterator->second.mUsers == 0 &&
                (tOldest == mTextures.end() || tIterator->second.mLastUse < tOldest->second.mLastUse))
            {
                tOldest = tIterator;
            }
        }
        if (tOldest == mTextures.end())
        {
            return;  // hepsi su an bir haritada ekranda
        }
        mTextureBytes -= tOldest->second.mBytes;
        mTextures.erase(tOldest);  // Gfx::Texture yikicisi dokuyu siler
    }
}

void TerrainHub::setTextureBudget(size_t pBytes)
{
    mTextureBudget = pBytes;
}

size_t TerrainHub::textureBudget() const
{
    return mTextureBudget;
}

size_t TerrainHub::textureBytes() const
{
    return mTextureBytes;
}

int TerrainHub::textureCount() const
{
    return static_cast<int>(mTextures.size());
}

bool TerrainHub::takeReadyVectors()
{
    bool tTaken = false;
    std::shared_ptr<const TerrainVectors> tVectors;
    while (mStore.popVectors(&tVectors))
    {
        CachedVectors& tEntry = mVectors[SceneryIndex::tileKey(tVectors->west(), tVectors->south())];
        tEntry.mVectors = tVectors;
        tTaken = true;
    }
    if (tTaken)
    {
        ++mVectorGeneration;
    }
    return tTaken;
}

uint64_t TerrainHub::vectorGeneration() const
{
    return mVectorGeneration;
}

std::shared_ptr<const TerrainVectors> TerrainHub::useVectors(int pWest, int pSouth, int pUser)
{
    const auto tFound = mVectors.find(SceneryIndex::tileKey(pWest, pSouth));
    if (tFound == mVectors.end())
    {
        mStore.requestVectors(pWest, pSouth);  // deniz / veri yok karoda sonuc hic gelmez (zararsiz)
        return nullptr;
    }
    tFound->second.mUsers |= bitOf(pUser);
    return tFound->second.mVectors;
}

void TerrainHub::releaseVectors(int pWest, int pSouth, int pUser)
{
    const auto tFound = mVectors.find(SceneryIndex::tileKey(pWest, pSouth));
    if (tFound != mVectors.end())
    {
        tFound->second.mUsers &= ~bitOf(pUser);
    }
    dropUnusedVectors();
}

void TerrainHub::dropUnusedVectors()
{
    // Kimsenin tutmadigi karolar: birkaci bekletilir (az once birakan harita / yeni acilan harita
    // hemen isteyebilir), fazlasi atilir ve isciye "tekrar istenebilir" denir
    size_t tIdle = 0;
    for (const auto& tEntry : mVectors)
    {
        tIdle += tEntry.second.mUsers == 0 ? 1 : 0;
    }
    for (auto tIterator = mVectors.begin(); tIterator != mVectors.end() && tIdle > kMaxIdleVectorTiles;)
    {
        if (tIterator->second.mUsers == 0)
        {
            mStore.forgetVectors(tIterator->second.mVectors->west(), tIterator->second.mVectors->south());
            tIterator = mVectors.erase(tIterator);
            --tIdle;
        }
        else
        {
            ++tIterator;
        }
    }
}
