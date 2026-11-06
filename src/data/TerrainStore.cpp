// TerrainStore.cpp
#include "data/TerrainStore.h"

#include <chrono>
#include <format>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

#include "data/DsfReader.h"
#include "data/SevenZip.h"
#include "data/TerrainRaster.h"
#include "data/TerrainTriangles.h"

namespace
{

const size_t kMaxRastersInMemory = 3;   // her biri ~17 MB
const size_t kMaxTriangleTiles = 2;     // yakin zoom ucgenleri, her biri ~30 MB
const uint64_t kStaleFrames = 120;       // bu kadar karedir istenmeyen istek atilir
const size_t kMaxQueuedResults = 64;     // ana thread almazsa isci bekler
const char* kCacheVersion = "terrain-v1";

using Clock = std::chrono::steady_clock;

// Onbellek anahtari: karonun butun aday DSF'leri (yol, boyut, degisiklik zamani)
std::string CacheKey(const std::vector<DsfCandidate>& pCandidates)
{
    std::string tKey = kCacheVersion;
    for (const DsfCandidate& tCandidate : pCandidates)
    {
        tKey += "|" + tCandidate.mPath + "|" + std::to_string(tCandidate.mSize) + "|" +
                std::to_string(tCandidate.mModified);
    }
    return tKey;
}

// "Temel mesh yok" sonucu da onbellege yazilir (yoksa sadece havalimani overlay'i
// olan binlerce karo her acilista yeniden acilirdi). Dosyada sadece anahtar var.
bool HasNoBaseMarker(const std::string& pPath, const std::string& pKey)
{
    std::ifstream tFile(pPath, std::ios::binary);
    if (!tFile)
    {
        return false;
    }
    const std::string tKey((std::istreambuf_iterator<char>(tFile)), std::istreambuf_iterator<char>());
    return tKey == pKey;
}

void WriteNoBaseMarker(const std::string& pPath, const std::string& pKey)
{
    std::ofstream tFile(pPath, std::ios::binary | std::ios::trunc);
    tFile << pKey;
}

bool ReadWholeFile(const std::string& pPath, std::vector<uint8_t>* pOut)
{
    std::ifstream tFile(pPath, std::ios::binary);
    if (!tFile)
    {
        return false;
    }
    pOut->assign(std::istreambuf_iterator<char>(tFile), std::istreambuf_iterator<char>());
    return true;
}

}  // namespace

int TerrainStore::ImageSize(int pLevel)
{
    const int kSizes[kLevelCount] = {64, 256, 1024, 1024, 1024, 1024};
    return kSizes[pLevel];
}

int TerrainStore::ChunksPerSide(int pLevel)
{
    if (pLevel == kDetailLevel)
    {
        return TerrainTriangles::kChunksPerSide;
    }
    if (pLevel == kFineLevel)
    {
        return TerrainTriangles::kChunksPerSide * 4;
    }
    return pLevel == 3 ? 4 : 1;
}

TerrainStore::TerrainStore() = default;

TerrainStore::~TerrainStore()
{
    Stop();
}

void TerrainStore::Start(const std::string& pXPlaneDirectory, const std::string& pCacheDirectory,
                         const TerrainPalette& pPalette, const std::vector<TerrainImageStyle>& pStyles)
{
    if (mThread.joinable())
    {
        return;
    }
    mXPlaneDirectory = pXPlaneDirectory;
    mCacheDirectory = pCacheDirectory;
    mPalette = pPalette;
    mStyles = pStyles;
    mStopping = false;
    mThread = std::thread(
        [this]()
        {
            WorkerLoop();
        });
}

void TerrainStore::Stop()
{
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        mStopping = true;
    }
    mWake.notify_all();
    if (mThread.joinable())
    {
        mThread.join();
    }
}

bool TerrainStore::IsIndexReady() const
{
    return mIndexReady.load();
}

TileCoverage TerrainStore::CoverageOf(int pWest, int pSouth) const
{
    if (!mIndexReady.load())
    {
        return TileCoverage::NoData;
    }
    return mIndex.CoverageOf(pWest, pSouth);  // dizin hazir olduktan sonra degismez
}

TerrainTileState TerrainStore::StateOf(int pWest, int pSouth) const
{
    std::lock_guard<std::mutex> tLock(mMutex);
    const auto tFound = mStates.find(SceneryIndex::TileKey(pWest, pSouth));
    return tFound == mStates.end() ? TerrainTileState::Unknown : tFound->second;
}

void TerrainStore::BeginFrame()
{
    std::lock_guard<std::mutex> tLock(mMutex);
    ++mFrame;
}

void TerrainStore::Request(const TerrainImageKey& pKey)
{
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        mRequests[pKey].mFrame = mFrame;
    }
    mWake.notify_one();
}

bool TerrainStore::PopImage(TerrainImage* pOut)
{
    std::lock_guard<std::mutex> tLock(mMutex);
    if (mResults.empty())
    {
        return false;
    }
    *pOut = std::move(mResults.front());
    mResults.pop_front();
    mWake.notify_one();  // kuyruk doluysa bekleyen isci devam etsin
    return true;
}

bool TerrainStore::IsIdle() const
{
    std::lock_guard<std::mutex> tLock(mMutex);
    return mRequests.empty() && mResults.empty() && mStatus.empty() && mIndexReady.load();
}

std::string TerrainStore::StatusText() const
{
    std::lock_guard<std::mutex> tLock(mMutex);
    return mStatus;
}

std::string TerrainStore::TakeLogLines()
{
    std::lock_guard<std::mutex> tLock(mMutex);
    std::string tLines;
    tLines.swap(mLog);
    return tLines;
}

void TerrainStore::AddLog(const std::string& pLine)
{
    std::lock_guard<std::mutex> tLock(mMutex);
    mLog += pLine + "\n";
}

void TerrainStore::SetState(int pWest, int pSouth, TerrainTileState pState)
{
    std::lock_guard<std::mutex> tLock(mMutex);
    mStates[SceneryIndex::TileKey(pWest, pSouth)] = pState;
}

bool TerrainStore::PickRequest(TerrainImageKey* pOut)
{
    // mMutex kilitliyken cagrilir. En son istenen; esitlikte kaba seviye once
    // (once bulanik ama hizli bir goruntu, sonra keskini).
    bool tFound = false;
    uint64_t tBestFrame = 0;
    int tBestLevel = 0;
    for (auto tIterator = mRequests.begin(); tIterator != mRequests.end();)
    {
        if (tIterator->second.mFrame + kStaleFrames < mFrame)
        {
            tIterator = mRequests.erase(tIterator);  // ekrandan cikmis
            continue;
        }
        const uint64_t tFrame = tIterator->second.mFrame;
        const int tLevel = tIterator->first.mLevel;
        if (!tFound || tFrame > tBestFrame || (tFrame == tBestFrame && tLevel < tBestLevel))
        {
            tFound = true;
            tBestFrame = tFrame;
            tBestLevel = tLevel;
            *pOut = tIterator->first;
        }
        ++tIterator;
    }
    if (tFound)
    {
        mRequests.erase(*pOut);
    }
    return tFound;
}

bool TerrainStore::BuildRaster(const SceneryIndex& pIndex, int pWest, int pSouth, const std::string& pCacheDirectory,
                               TerrainRaster* pOut, std::string* pLog, TerrainTileState* pState)
{
    // Temel mesh yoksa: bolge kuruluysa deniz, degilse veri yok
    const TerrainTileState tNoBaseState =
        pIndex.IsRegionInstalled(pWest, pSouth) ? TerrainTileState::Ocean : TerrainTileState::NoData;
    const std::vector<DsfCandidate>* tCandidates = pIndex.CandidatesFor(pWest, pSouth);
    if (!tCandidates)
    {
        *pState = tNoBaseState;
        return false;
    }
    const std::string tName = SceneryIndex::TileName(pWest, pSouth);
    const std::string tCachePath = pCacheDirectory + tName + ".terrain";
    const std::string tNoBasePath = pCacheDirectory + tName + ".nobase";
    const std::string tKey = CacheKey(*tCandidates);
    if (!pCacheDirectory.empty() && pOut->Load(tCachePath, tKey))
    {
        *pState = TerrainTileState::Ready;
        return true;  // onbellek guncel: DSF'e hic dokunulmadi
    }
    if (!pCacheDirectory.empty() && HasNoBaseMarker(tNoBasePath, tKey))
    {
        *pState = tNoBaseState;  // daha once bakildi: bu karoda sadece overlay var
        return false;
    }

    const Clock::time_point tStart = Clock::now();
    std::vector<uint8_t> tData;
    DsfReader tReader;
    std::string tPath;
    if (!OpenBaseMesh(*tCandidates, tName, &tData, &tReader, &tPath, pLog))
    {
        // Sadece overlay DSF'leri var (veya hepsi okunamadi): temel mesh yok
        *pState = tNoBaseState;
        if (!pCacheDirectory.empty())
        {
            WriteNoBaseMarker(tNoBasePath, tKey);
        }
        *pLog += "terrain " + tName + ": no base mesh (only overlays)\n";
        return false;
    }

    // Arazi turu indeksi -> sinif
    std::vector<TerrainClass> tClasses;
    for (const std::string& tTerrain : tReader.Info().mTerrainNames)
    {
        tClasses.push_back(ClassifyTerrain(tTerrain));
    }
    pOut->Reset(pWest, pSouth);
    std::string tError;
    const bool tOk = tReader.ForEachTriangle(
        [&](const DsfTriangle& pTriangle)
        {
            if ((pTriangle.mPatchFlags & DsfReader::kPatchFlagOverlay) == 0 && pTriangle.mTerrain >= 0 &&
                pTriangle.mTerrain < static_cast<int>(tClasses.size()))
            {
                pOut->FillTriangle(pTriangle.mLon, pTriangle.mLat, tClasses[pTriangle.mTerrain]);
            }
        },
        &tError);
    if (!tOk)
    {
        *pLog += "terrain " + tName + ": " + tError + " (" + tPath + ")\n";
        *pState = TerrainTileState::Failed;
        return false;
    }
    DsfRaster tElevation;
    if (tReader.ReadRaster("elevation", &tElevation))
    {
        pOut->ComputeShading(tElevation);
    }
    if (!pCacheDirectory.empty() && !pOut->Save(tCachePath, tKey))
    {
        *pLog += "terrain " + tName + ": cannot write cache " + tCachePath + "\n";
    }
    *pLog += std::format("terrain {}: built from {} in {:.1f} s (water {:.0f}%, relief {})\n", tName, tPath,
                         std::chrono::duration<double>(Clock::now() - tStart).count(),
                         100.0 * pOut->CountOf(TerrainClass::Water) / (TerrainRaster::kSize * TerrainRaster::kSize),
                         pOut->HasShading() ? "yes" : "no");
    *pState = TerrainTileState::Ready;
    return true;
}

bool TerrainStore::OpenBaseMesh(const std::vector<DsfCandidate>& pCandidates, const std::string& pName,
                                std::vector<uint8_t>* pData, DsfReader* pReader, std::string* pPath,
                                std::string* pLog)
{
    // Oncelik sirasiyla: overlay olmayan ilk DSF karonun temel mesh'idir
    for (const DsfCandidate& tCandidate : pCandidates)
    {
        if (!ReadWholeFile(tCandidate.mPath, pData))
        {
            *pLog += "terrain " + pName + ": cannot read " + tCandidate.mPath + "\n";
            continue;
        }
        std::string tError;
        if (SevenZip::IsArchive(pData->data(), pData->size()))
        {
            std::vector<uint8_t> tPlain;
            if (!SevenZip::ExtractFirstFile(*pData, &tPlain, &tError))
            {
                *pLog += "terrain " + pName + ": " + tError + " (" + tCandidate.mPath + ")\n";
                continue;
            }
            pData->swap(tPlain);
        }
        if (!pReader->Open(*pData, &tError))
        {
            *pLog += "terrain " + pName + ": " + tError + " (" + tCandidate.mPath + ")\n";
            continue;
        }
        if (pReader->Info().mOverlay)
        {
            continue;  // ustune serilen katman (havalimani, nesneler): zemin degil
        }
        *pPath = tCandidate.mPath;
        return true;
    }
    return false;
}

bool TerrainStore::LoadTriangles(const SceneryIndex& pIndex, int pWest, int pSouth, TerrainTriangles* pOut,
                                 std::string* pLog)
{
    const std::vector<DsfCandidate>* tCandidates = pIndex.CandidatesFor(pWest, pSouth);
    if (!tCandidates)
    {
        return false;
    }
    const std::string tName = SceneryIndex::TileName(pWest, pSouth);
    const Clock::time_point tStart = Clock::now();
    std::vector<uint8_t> tData;
    DsfReader tReader;
    std::string tPath;
    if (!OpenBaseMesh(*tCandidates, tName, &tData, &tReader, &tPath, pLog))
    {
        return false;
    }
    std::vector<TerrainClass> tClasses;
    for (const std::string& tTerrain : tReader.Info().mTerrainNames)
    {
        tClasses.push_back(ClassifyTerrain(tTerrain));
    }
    pOut->Reset(pWest, pSouth);
    std::string tError;
    const bool tOk = tReader.ForEachTriangle(
        [&](const DsfTriangle& pTriangle)
        {
            if ((pTriangle.mPatchFlags & DsfReader::kPatchFlagOverlay) == 0 && pTriangle.mTerrain >= 0 &&
                pTriangle.mTerrain < static_cast<int>(tClasses.size()))
            {
                pOut->Add(pTriangle.mLon, pTriangle.mLat, tClasses[pTriangle.mTerrain]);
            }
        },
        &tError);
    if (!tOk)
    {
        *pLog += "terrain detail " + tName + ": " + tError + " (" + tPath + ")\n";
        return false;
    }
    *pLog += std::format("terrain detail {}: {} triangles, {:.0f} MB in {:.1f} s\n", tName, pOut->TriangleCount(),
                         pOut->MemoryBytes() / 1048576.0,
                         std::chrono::duration<double>(Clock::now() - tStart).count());
    return true;
}

TerrainTriangles* TerrainStore::TrianglesFor(int pWest, int pSouth)
{
    const int tKey = SceneryIndex::TileKey(pWest, pSouth);
    for (auto tIterator = mTriangles.begin(); tIterator != mTriangles.end(); ++tIterator)
    {
        if (tIterator->first == tKey)
        {
            mTriangles.splice(mTriangles.begin(), mTriangles, tIterator);  // en son kullanilan basa
            return mTriangles.front().second.get();
        }
    }
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        mStatus = "Loading terrain detail " + SceneryIndex::TileName(pWest, pSouth);
    }
    std::unique_ptr<TerrainTriangles> tTriangles = std::make_unique<TerrainTriangles>();
    std::string tLog;
    const bool tOk = LoadTriangles(mIndex, pWest, pSouth, tTriangles.get(), &tLog);
    if (!tLog.empty())
    {
        tLog.pop_back();
        AddLog(tLog);
    }
    if (!tOk)
    {
        return nullptr;
    }
    mTriangles.emplace_front(tKey, std::move(tTriangles));
    if (mTriangles.size() > kMaxTriangleTiles)
    {
        mTriangles.pop_back();
    }
    return mTriangles.front().second.get();
}

TerrainRaster* TerrainStore::RasterFor(int pWest, int pSouth)
{
    const int tKey = SceneryIndex::TileKey(pWest, pSouth);
    for (auto tIterator = mRasters.begin(); tIterator != mRasters.end(); ++tIterator)
    {
        if (tIterator->first == tKey)
        {
            mRasters.splice(mRasters.begin(), mRasters, tIterator);  // en son kullanilan basa
            return mRasters.front().second.get();
        }
    }
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        mStatus = "Building terrain " + SceneryIndex::TileName(pWest, pSouth);
    }
    std::unique_ptr<TerrainRaster> tRaster = std::make_unique<TerrainRaster>();
    std::string tLog;
    TerrainTileState tState = TerrainTileState::Unknown;
    const bool tOk = BuildRaster(mIndex, pWest, pSouth, mCacheDirectory, tRaster.get(), &tLog, &tState);
    if (!tLog.empty())
    {
        tLog.pop_back();
        AddLog(tLog);
    }
    SetState(pWest, pSouth, tState);
    if (!tOk)
    {
        return nullptr;
    }
    mRasters.emplace_front(tKey, std::move(tRaster));
    if (mRasters.size() > kMaxRastersInMemory)
    {
        mRasters.pop_back();
    }
    return mRasters.front().second.get();
}

void TerrainStore::WorkerLoop()
{
    // ARKA PLAN THREAD'i: XPLM yok. Log satirlari biriktirilir, ana thread yazar.
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        mStatus = "Indexing scenery...";
    }
    std::error_code tError;
    std::filesystem::create_directories(mCacheDirectory, tError);
    const Clock::time_point tStart = Clock::now();
    mIndex.Build(mXPlaneDirectory);
    mIndexReady.store(true);
    AddLog(std::format("terrain index: {} DSF files in {} packs, {} tiles ({:.1f} s)", mIndex.FileCount(),
                       mIndex.PackCount(), mIndex.TileCount(),
                       std::chrono::duration<double>(Clock::now() - tStart).count()));

    while (true)
    {
        TerrainImageKey tKey;
        {
            std::unique_lock<std::mutex> tLock(mMutex);
            mStatus.clear();
            mWake.wait(tLock,
                       [this]()
                       {
                           return mStopping || (!mRequests.empty() && mResults.size() < kMaxQueuedResults);
                       });
            if (mStopping)
            {
                return;
            }
            if (!PickRequest(&tKey))
            {
                continue;
            }
            const auto tState = mStates.find(SceneryIndex::TileKey(tKey.mWest, tKey.mSouth));
            if (tState != mStates.end() && tState->second != TerrainTileState::Ready &&
                tState->second != TerrainTileState::Unknown)
            {
                continue;  // deniz / veri yok / bozuk: goruntu uretilmez
            }
        }

        const TerrainRaster* tRaster = RasterFor(tKey.mWest, tKey.mSouth);
        if (!tRaster)
        {
            continue;
        }
        TerrainImage tImage;
        tImage.mKey = tKey;
        tImage.mSize = ImageSize(tKey.mLevel);
        const TerrainImageStyle tStyle = tKey.mStyle >= 0 && tKey.mStyle < static_cast<int>(mStyles.size())
                                             ? mStyles[tKey.mStyle]
                                             : TerrainImageStyle();
        if (tKey.mLevel >= kDetailLevel)
        {
            // Yakin zoom: parca raster'dan degil, dogrudan DSF ucgenlerinden boyanir
            const TerrainTriangles* tTriangles = TrianglesFor(tKey.mWest, tKey.mSouth);
            if (!tTriangles)
            {
                continue;
            }
            tRaster->BuildDetailImage(*tTriangles, ChunksPerSide(tKey.mLevel), tKey.mChunk, tImage.mSize, mPalette,
                                      tStyle, &tImage.mRgba);
        }
        else
        {
            const int tChunks = ChunksPerSide(tKey.mLevel);
            const int tSpan = TerrainRaster::kSize / tChunks;
            const int tX0 = (tKey.mChunk % tChunks) * tSpan;
            const int tY0 = (tKey.mChunk / tChunks) * tSpan;
            tRaster->BuildImage(tX0, tY0, tX0 + tSpan, tY0 + tSpan, tImage.mSize, mPalette, tStyle, &tImage.mRgba);
        }
        TerrainRaster::BleedTransparent(tImage.mSize, &tImage.mRgba);  // mipmap kenarlari koyulasmasin

        std::lock_guard<std::mutex> tLock(mMutex);
        mResults.push_back(std::move(tImage));
    }
}
