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
#include "data/TerrainElevation.h"
#include "data/TerrainRaster.h"
#include "data/TerrainTriangles.h"
#include "data/TerrainVectors.h"

namespace
{

const size_t kMaxRastersInMemory = 3;   // her biri ~17 MB
const size_t kMaxTriangleTiles = 2;     // yakin zoom ucgenleri, her biri ~30 MB
const size_t kMaxElevationTiles = 12;        // tam rakim, her biri ~5 MB; bir goruntu 9 karo ister (3 x 3)
const size_t kMaxCoarseElevationTiles = 64;  // kaba rakim (seviye 2+), her biri ~0,5 MB; uzak zoom
const int kCoarseElevationLevel = 2;         // bu seviye ve ustu kaba izgaradan okunabilir (~370 m)
const size_t kMaxVectorTiles = 4;            // vektorler, karo basina ~5-10 MB
const uint64_t kStaleFrames = 120;       // bu kadar karedir istenmeyen istek atilir
const size_t kMaxQueuedResults = 64;     // ana thread almazsa isci bekler
const char* kCacheVersion = "terrain-v1";

using Clock = std::chrono::steady_clock;

double MillisecondsSince(Clock::time_point pStart)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - pStart).count();
}

int WrapWest(int pWest)
{
    return pWest < -180 ? pWest + 360 : (pWest > 179 ? pWest - 360 : pWest);
}

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
        if (mHasInFlight && mInFlight == pKey)
        {
            return;  // isci su an bunu uretiyor
        }
        for (const TerrainImage& tResult : mResults)
        {
            if (tResult.mKey == pKey)
            {
                return;  // hazir, ana thread henuz almadi
            }
        }
        mRequests[pKey].mFrame = mFrame;
    }
    mWake.notify_one();
}

bool TerrainStore::HasElevation(int pWest, int pSouth) const
{
    std::lock_guard<std::mutex> tLock(mMutex);
    return mElevationReady.count(SceneryIndex::TileKey(pWest, pSouth)) > 0;
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
    // Uretilmekte olan goruntu de is sayilir: katman ayni istegi tekrarlasa bile kuyruga girmez (Request)
    return mRequests.empty() && mResults.empty() && mVectorRequests.empty() && mVectorResults.empty() &&
           !mHasInFlight && mStatus.empty() && mIndexReady.load();
}

void TerrainStore::RequestVectors(int pWest, int pSouth)
{
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        const int tKey = SceneryIndex::TileKey(pWest, pSouth);
        if (mVectorsServed.count(tKey))
        {
            return;  // verildi; katman kendi kopyasini tutar (atinca ForgetVectors ile yeniden ister)
        }
        mVectorRequests[tKey] = mFrame;
    }
    mWake.notify_one();
}

bool TerrainStore::PopVectors(std::shared_ptr<const TerrainVectors>* pOut)
{
    std::lock_guard<std::mutex> tLock(mMutex);
    if (mVectorResults.empty())
    {
        return false;
    }
    *pOut = std::move(mVectorResults.front());
    mVectorResults.pop_front();
    return true;
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
                               TerrainRaster* pOut, std::string* pLog, TerrainTileState* pState,
                               ElevationGrid* pElevation, TerrainVectors* pVectors)
{
    // Temel mesh yoksa: deniz ya da veri yok (SceneryIndex: dunya tablosu, yoksa 10 derece klasor kurali)
    const TerrainTileState tNoBaseState = pIndex.NoBaseCoverage(pWest, pSouth) == TileCoverage::Ocean
                                              ? TerrainTileState::Ocean
                                              : TerrainTileState::NoData;
    const std::vector<DsfCandidate>* tCandidates = pIndex.CandidatesFor(pWest, pSouth);
    if (!tCandidates)
    {
        *pState = tNoBaseState;
        return false;
    }
    const std::string tName = SceneryIndex::TileName(pWest, pSouth);
    const std::string tCachePath = pCacheDirectory + tName + ".terrain";
    const std::string tElevationPath = pCacheDirectory + tName + ".elevation";
    const std::string tVectorsPath = pCacheDirectory + tName + ".vectors";
    const std::string tNoBasePath = pCacheDirectory + tName + ".nobase";
    const std::string tKey = CacheKey(*tCandidates);
    if (!pCacheDirectory.empty() && pOut->Load(tCachePath, tKey) &&
        (!pElevation || pElevation->Load(tElevationPath, tKey)) && (!pVectors || pVectors->Load(tVectorsPath, tKey)))
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
    // Vektorler (Asama 2): temel mesh'in poligon / zincirleri + overlay DSF'lerinin dislama alanlari
    TerrainVectors tLocalVectors;
    TerrainVectors& tVectors = pVectors ? *pVectors : tLocalVectors;
    tVectors.Reset(pWest, pSouth);
    std::string tVectorError;
    if (!tVectors.AddFromDsf(tReader, &tVectorError))
    {
        *pLog += "terrain " + tName + ": vectors skipped: " + tVectorError + "\n";
        tVectors.Reset(pWest, pSouth);
    }
    for (const DsfCandidate& tCandidate : *tCandidates)
    {
        if (tCandidate.mPath == tPath)
        {
            continue;  // temel mesh
        }
        std::vector<uint8_t> tOverlayData;
        DsfReader tOverlay;
        std::string tOverlayError;
        if (!ReadWholeFile(tCandidate.mPath, &tOverlayData))
        {
            continue;
        }
        if (SevenZip::IsArchive(tOverlayData.data(), tOverlayData.size()))
        {
            std::vector<uint8_t> tPlain;
            if (!SevenZip::ExtractFirstFile(tOverlayData, &tPlain, &tOverlayError))
            {
                continue;
            }
            tOverlayData.swap(tPlain);
        }
        if (tOverlay.Open(tOverlayData, &tOverlayError) && tOverlay.Info().mOverlay)
        {
            tVectors.AddExclusions(tOverlay.Info());
        }
    }
    tVectors.ApplyNetworkExclusions();
    pOut->FillAreas(tVectors);
    if (!pCacheDirectory.empty() && !tVectors.Save(tVectorsPath, tKey))
    {
        *pLog += "terrain " + tName + ": cannot write cache " + tVectorsPath + "\n";
    }

    if (!pCacheDirectory.empty() && !pOut->Save(tCachePath, tKey))
    {
        *pLog += "terrain " + tName + ": cannot write cache " + tCachePath + "\n";
    }

    // Rakim: tam cozunurluklu DEM + sea_level ayri dosyaya (DEM yoksa bos dosya: kabartmasiz karo)
    std::string tElevationText = "no elevation";
    if (pElevation)
    {
        DsfRaster tDem;
        DsfRaster tSeaLevel;
        const bool tHasDem = tReader.ReadRaster("elevation", &tDem);
        const bool tHasSea = tReader.ReadRaster("sea_level", &tSeaLevel);
        if (!tHasDem || !pElevation->Build(tDem, tHasSea ? &tSeaLevel : nullptr))
        {
            *pElevation = ElevationGrid();
        }
        const Clock::time_point tPackStart = Clock::now();
        if (!pCacheDirectory.empty() && !pElevation->Save(tElevationPath, tKey))
        {
            *pLog += "terrain " + tName + ": cannot write cache " + tElevationPath + "\n";
        }
        if (!pElevation->IsEmpty())
        {
            std::error_code tSizeError;
            const uintmax_t tBytes = std::filesystem::file_size(tElevationPath, tSizeError);
            tElevationText = std::format("elevation {} posts, sea_level {}, packed {:.2f} MB in {:.0f} ms",
                                         pElevation->Size(0), tHasSea ? "yes" : "no",
                                         tSizeError ? 0.0 : tBytes / 1048576.0, MillisecondsSince(tPackStart));
        }
    }
    *pLog += std::format("terrain {}: built from {} in {:.1f} s (water {:.0f}%, {}, vectors {} areas {} lines)\n",
                         tName, tPath, std::chrono::duration<double>(Clock::now() - tStart).count(),
                         100.0 * pOut->CountOf(TerrainClass::Water) / (TerrainRaster::kSize * TerrainRaster::kSize),
                         tElevationText, tVectors.Areas().size(), tVectors.Lines().size());
    *pState = TerrainTileState::Ready;
    return true;
}

bool TerrainStore::LoadElevation(const SceneryIndex& pIndex, int pWest, int pSouth,
                                 const std::string& pCacheDirectory, ElevationGrid* pOut)
{
    const std::vector<DsfCandidate>* tCandidates = pIndex.CandidatesFor(pWest, pSouth);
    if (!tCandidates || pCacheDirectory.empty())
    {
        return false;
    }
    const std::string tPath = pCacheDirectory + SceneryIndex::TileName(pWest, pSouth) + ".elevation";
    return pOut->Load(tPath, CacheKey(*tCandidates));
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
    std::unique_ptr<ElevationGrid> tElevation = std::make_unique<ElevationGrid>();
    std::shared_ptr<TerrainVectors> tVectors = std::make_shared<TerrainVectors>();
    std::string tLog;
    TerrainTileState tState = TerrainTileState::Unknown;
    const bool tOk = BuildRaster(mIndex, pWest, pSouth, mCacheDirectory, tRaster.get(), &tLog, &tState,
                                 tElevation.get(), tVectors.get());
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
    KeepElevation(pWest, pSouth, std::move(tElevation));
    mVectors.emplace_front(tKey, std::move(tVectors));
    if (mVectors.size() > kMaxVectorTiles)
    {
        mVectors.pop_back();
    }
    mRasters.emplace_front(tKey, std::move(tRaster));
    if (mRasters.size() > kMaxRastersInMemory)
    {
        mRasters.pop_back();
    }
    return mRasters.front().second.get();
}

void TerrainStore::KeepElevation(int pWest, int pSouth, std::unique_ptr<ElevationGrid> pGrid)
{
    const int tKey = SceneryIndex::TileKey(pWest, pSouth);
    auto tRemove = [tKey](std::list<std::pair<int, std::unique_ptr<ElevationGrid>>>& pList)
    {
        for (auto tIterator = pList.begin(); tIterator != pList.end(); ++tIterator)
        {
            if (tIterator->first == tKey)
            {
                pList.erase(tIterator);
                return;
            }
        }
    };
    tRemove(mElevations);
    tRemove(mCoarseElevations);  // tam izgara varken kaba kopyasi gereksiz
    mElevations.emplace_front(tKey, std::move(pGrid));
    if (mElevations.size() > kMaxElevationTiles)
    {
        // En eski tam izgara silinmez: kucultulup kaba listeye gecer (uzak zoom'da yeniden diskten okunmasin)
        std::pair<int, std::unique_ptr<ElevationGrid>> tOldest = std::move(mElevations.back());
        mElevations.pop_back();
        tOldest.second->DropFineLevels(kCoarseElevationLevel);
        KeepCoarseElevation(tOldest.first, std::move(tOldest.second));
    }
    std::lock_guard<std::mutex> tLock(mMutex);
    mElevationReady.insert(tKey);
}

void TerrainStore::KeepCoarseElevation(int pKey, std::unique_ptr<ElevationGrid> pGrid)
{
    mCoarseElevations.emplace_front(pKey, std::move(pGrid));
    if (mCoarseElevations.size() > kMaxCoarseElevationTiles)
    {
        mCoarseElevations.pop_back();
    }
    std::lock_guard<std::mutex> tLock(mMutex);
    mElevationReady.insert(pKey);
}

const ElevationGrid* TerrainStore::ElevationFor(int pWest, int pSouth, int pLevel)
{
    const int tKey = SceneryIndex::TileKey(pWest, pSouth);
    // Bellekte: once tam izgara, kaba seviye yetiyorsa kaba izgara
    auto tFind = [tKey](std::list<std::pair<int, std::unique_ptr<ElevationGrid>>>& pList) -> const ElevationGrid*
    {
        for (auto tIterator = pList.begin(); tIterator != pList.end(); ++tIterator)
        {
            if (tIterator->first == tKey)
            {
                pList.splice(pList.begin(), pList, tIterator);  // en son kullanilan basa
                return pList.front().second.get();
            }
        }
        return nullptr;
    };
    const bool tCoarseIsEnough = pLevel >= kCoarseElevationLevel;
    if (const ElevationGrid* tFull = tFind(mElevations))
    {
        return tFull;
    }
    if (tCoarseIsEnough)
    {
        if (const ElevationGrid* tCoarse = tFind(mCoarseElevations))
        {
            return tCoarse;
        }
    }
    // Sadece onbellekten (komsu icin DSF acilmaz: o karo kendi sirasi gelince uretilir)
    std::unique_ptr<ElevationGrid> tGrid = std::make_unique<ElevationGrid>();
    const Clock::time_point tStart = Clock::now();
    if (!LoadElevation(mIndex, pWest, pSouth, mCacheDirectory, tGrid.get()))
    {
        return nullptr;
    }
    AddLog(std::format("terrain {}: elevation from cache in {:.0f} ms ({})", SceneryIndex::TileName(pWest, pSouth),
                       MillisecondsSince(tStart), tCoarseIsEnough ? "coarse" : "full"));
    if (tCoarseIsEnough)
    {
        tGrid->DropFineLevels(kCoarseElevationLevel);  // uzak zoom: tam izgara listesini doldurmasin
        KeepCoarseElevation(tKey, std::move(tGrid));
        return mCoarseElevations.front().second.get();
    }
    KeepElevation(pWest, pSouth, std::move(tGrid));
    return mElevations.front().second.get();
}

void TerrainStore::BuildImageFor(const TerrainImageKey& pKey, const TerrainRaster& pRaster, TerrainImage* pOut)
{
    // Rakim: karo ve 8 komsusu. Komsu sadece goruntu o kenara degiyorsa gerekir.
    const int tChunks = ChunksPerSide(pKey.mLevel);
    const int tChunkColumn = pKey.mChunk % tChunks;
    const int tChunkRow = pKey.mChunk / tChunks;
    const bool tTouches[3][3] = {
        {tChunkRow == 0 && tChunkColumn == 0, tChunkRow == 0, tChunkRow == 0 && tChunkColumn == tChunks - 1},
        {tChunkColumn == 0, true, tChunkColumn == tChunks - 1},
        {tChunkRow == tChunks - 1 && tChunkColumn == 0, tChunkRow == tChunks - 1,
         tChunkRow == tChunks - 1 && tChunkColumn == tChunks - 1},
    };
    // Goruntunun okuyacagi piramit seviyesi (ComposeImage ile ayni piksel boyu: 1 derece / (parca x boyut))
    const double tMetersPerPixel = 111320.0 / (static_cast<double>(tChunks) * pOut->mSize);
    const int tLevel = ElevationGrid::NominalLevelFor(tMetersPerPixel);
    ElevationTiles tTiles;
    tTiles.mTiles[1][1] = ElevationFor(pKey.mWest, pKey.mSouth, tLevel);
    for (int tRow = 0; tRow < 3 && tTiles.mTiles[1][1]; ++tRow)
    {
        for (int tColumn = 0; tColumn < 3; ++tColumn)
        {
            const int tWest = WrapWest(pKey.mWest + tColumn - 1);
            const int tSouth = pKey.mSouth + 1 - tRow;  // satir 0 = kuzey komsu
            if ((tRow == 1 && tColumn == 1) || !tTouches[tRow][tColumn] || tSouth < -90 || tSouth > 89 ||
                mIndex.CoverageOf(tWest, tSouth) != TileCoverage::HasDsf)
            {
                continue;  // gerekmiyor ya da hic olmayacak (deniz / veri yok): tek tarafli fark kalici
            }
            tTiles.mTiles[tRow][tColumn] = ElevationFor(tWest, tSouth, tLevel);
            // Eksik say: sadece henuz hic hazir olmamis komsu. Bir kez hazir olup simdi bulunamayan
            // (bellekten atilmis, onbellege yazilamamis) komsu beklenmez: yoksa katman bu goruntuyu
            // her karede yeniden ister, sonsuz yeniden uretim olur.
            if (!tTiles.mTiles[tRow][tColumn] && !HasElevation(tWest, tSouth))
            {
                pOut->mMissingNeighbors.push_back(SceneryIndex::TileKey(tWest, tSouth));
            }
        }
    }
    // Isaretciler bu goruntu boyunca gecerli kalir: tam liste 12 >= 9; tam listeden atilan izgara
    // silinmez, kaba listeye tasinir (ayni nesne, sadece ince seviyeleri birakilir; bu goruntu
    // kaba seviye okuyorsa sorun yok, ince seviye okuyorsa 9 karo zaten tam listede sigar)
    const ElevationTiles* tElevation = tTiles.mTiles[1][1] ? &tTiles : nullptr;

    const TerrainImageStyle tStyle = pKey.mStyle >= 0 && pKey.mStyle < static_cast<int>(mStyles.size())
                                         ? mStyles[pKey.mStyle]
                                         : TerrainImageStyle();
    if (pKey.mLevel >= kDetailLevel)
    {
        // Yakin zoom: parca raster'dan degil, dogrudan DSF ucgenlerinden boyanir
        const TerrainTriangles* tTriangles = TrianglesFor(pKey.mWest, pKey.mSouth);
        if (!tTriangles)
        {
            pOut->mRgba.clear();
            return;
        }
        const std::shared_ptr<TerrainVectors> tVectors =
            pKey.mAreas != 0 ? VectorsFor(pKey.mWest, pKey.mSouth) : nullptr;
        pRaster.BuildDetailImage(*tTriangles, tChunks, pKey.mChunk, pOut->mSize, mPalette, tStyle, &pOut->mRgba,
                                 tElevation, tVectors.get(), pKey.mAreas);
        return;
    }
    const int tSpan = TerrainRaster::kSize / tChunks;
    const int tX0 = tChunkColumn * tSpan;
    const int tY0 = tChunkRow * tSpan;
    pRaster.BuildImage(tX0, tY0, tX0 + tSpan, tY0 + tSpan, pOut->mSize, mPalette, tStyle, &pOut->mRgba, tElevation,
                       pKey.mAreas);
}

std::shared_ptr<TerrainVectors> TerrainStore::VectorsFor(int pWest, int pSouth)
{
    const int tKey = SceneryIndex::TileKey(pWest, pSouth);
    for (auto tIterator = mVectors.begin(); tIterator != mVectors.end(); ++tIterator)
    {
        if (tIterator->first == tKey)
        {
            mVectors.splice(mVectors.begin(), mVectors, tIterator);
            return mVectors.front().second;
        }
    }
    // Onbellekten (raster ile her zaman birlikte yazilir). Raster bellekten SILINMEZ: cagiran (yakin zoom
    // goruntusu) o anda ona basvuruyor olabilir.
    std::shared_ptr<TerrainVectors> tVectors = std::make_shared<TerrainVectors>();
    if (!LoadVectors(mIndex, pWest, pSouth, mCacheDirectory, tVectors.get()))
    {
        // Onbellek yoksa karo henuz uretilmemistir: uretilirken (RasterFor) vektorler de gelir
        if (!RasterFor(pWest, pSouth) || mVectors.empty() || mVectors.front().first != tKey)
        {
            return nullptr;
        }
        return mVectors.front().second;
    }
    mVectors.emplace_front(tKey, tVectors);
    if (mVectors.size() > kMaxVectorTiles)
    {
        mVectors.pop_back();
    }
    return tVectors;
}

bool TerrainStore::LoadVectors(const SceneryIndex& pIndex, int pWest, int pSouth, const std::string& pCacheDirectory,
                               TerrainVectors* pOut)
{
    const std::vector<DsfCandidate>* tCandidates = pIndex.CandidatesFor(pWest, pSouth);
    if (!tCandidates || pCacheDirectory.empty())
    {
        return false;
    }
    const std::string tPath = pCacheDirectory + SceneryIndex::TileName(pWest, pSouth) + ".vectors";
    return pOut->Load(tPath, CacheKey(*tCandidates));
}

void TerrainStore::ForgetVectors(int pWest, int pSouth)
{
    std::lock_guard<std::mutex> tLock(mMutex);
    mVectorsServed.erase(SceneryIndex::TileKey(pWest, pSouth));
}

bool TerrainStore::ServeVectorRequest()
{
    int tKey = 0;
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        // Bayat istekleri at, en son istenen once
        uint64_t tBestFrame = 0;
        bool tFound = false;
        for (auto tIterator = mVectorRequests.begin(); tIterator != mVectorRequests.end();)
        {
            if (tIterator->second + kStaleFrames < mFrame)
            {
                tIterator = mVectorRequests.erase(tIterator);
                continue;
            }
            if (!tFound || tIterator->second > tBestFrame)
            {
                tFound = true;
                tBestFrame = tIterator->second;
                tKey = tIterator->first;
            }
            ++tIterator;
        }
        if (!tFound)
        {
            return false;
        }
        mVectorRequests.erase(tKey);
        mVectorsServed.insert(tKey);
        const auto tState = mStates.find(tKey);
        if (tState != mStates.end() && tState->second != TerrainTileState::Ready &&
            tState->second != TerrainTileState::Unknown)
        {
            return true;  // deniz / veri yok: vektor yok
        }
    }
    const std::shared_ptr<TerrainVectors> tVectors = VectorsFor(tKey % 360 - 180, tKey / 360 - 90);
    if (tVectors)
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        mVectorResults.push_back(tVectors);
    }
    return true;
}

void TerrainStore::NoteImageTime(int pLevel, double pMilliseconds)
{
    if (pLevel >= 0 && pLevel < kLevelCount)
    {
        ++mImageCounts[pLevel];
        mImageMilliseconds[pLevel] += pMilliseconds;
    }
}

void TerrainStore::LogImageTimes()
{
    // Is bitti (kuyruk bos): bu turdaki goruntu surelerinin ozeti, seviye basina ortalama
    std::string tText;
    for (int tLevel = 0; tLevel < kLevelCount; ++tLevel)
    {
        if (mImageCounts[tLevel] > 0)
        {
            tText += std::format(" L{} {} x {:.0f} ms", tLevel, mImageCounts[tLevel],
                                 mImageMilliseconds[tLevel] / mImageCounts[tLevel]);
            mImageCounts[tLevel] = 0;
            mImageMilliseconds[tLevel] = 0.0;
        }
    }
    if (!tText.empty())
    {
        AddLog("terrain images (count x average):" + tText);
    }
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
    AddLog(mIndex.WorldMapReport());

    while (true)
    {
        TerrainImageKey tKey;
        bool tIdle = false;
        {
            std::lock_guard<std::mutex> tLock(mMutex);
            mHasInFlight = false;
            tIdle = mRequests.empty() && mVectorRequests.empty();
        }
        if (ServeVectorRequest())
        {
            continue;  // vektor istekleri kucuk ve yol katmani bekliyor: once onlar
        }
        if (tIdle)
        {
            LogImageTimes();  // bekleme oncesi: bu turun ozeti
        }
        {
            std::unique_lock<std::mutex> tLock(mMutex);
            mStatus.clear();
            mWake.wait(tLock,
                       [this]()
                       {
                           return mStopping || !mVectorRequests.empty() ||
                                  (!mRequests.empty() && mResults.size() < kMaxQueuedResults);
                       });
            if (mStopping)
            {
                return;
            }
            if (!PickRequest(&tKey))
            {
                continue;  // sadece vektor istegi vardi (dongunun basinda karsilanir)
            }
            const auto tState = mStates.find(SceneryIndex::TileKey(tKey.mWest, tKey.mSouth));
            if (tState != mStates.end() && tState->second != TerrainTileState::Ready &&
                tState->second != TerrainTileState::Unknown)
            {
                continue;  // deniz / veri yok / bozuk: goruntu uretilmez
            }
            mHasInFlight = true;
            mInFlight = tKey;
        }

        const TerrainRaster* tRaster = RasterFor(tKey.mWest, tKey.mSouth);
        if (!tRaster)
        {
            continue;
        }
        TerrainImage tImage;
        tImage.mKey = tKey;
        tImage.mSize = ImageSize(tKey.mLevel);
        const Clock::time_point tImageStart = Clock::now();
        BuildImageFor(tKey, *tRaster, &tImage);
        if (tImage.mRgba.empty())
        {
            continue;  // yakin zoom ucgenleri okunamadi
        }
        NoteImageTime(tKey.mLevel, MillisecondsSince(tImageStart));
        TerrainRaster::BleedTransparent(tImage.mSize, &tImage.mRgba);  // mipmap kenarlari koyulasmasin

        std::lock_guard<std::mutex> tLock(mMutex);
        mResults.push_back(std::move(tImage));
    }
}
