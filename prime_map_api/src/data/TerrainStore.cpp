#include "data/TerrainStore.h"
#include "data/TerrainProfile.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <optional>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

#include "data/CacheTrim.h"
#include "data/AtomicFile.h"
#include "data/DsfReader.h"
#include "data/SevenZip.h"
#include "data/TerrainElevation.h"
#include "data/TerrainRaster.h"
#include "data/TerrainTriangles.h"
#include "data/TerrainVectors.h"

#if defined(_WIN32)
#include <windows.h>
#endif

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

double millisecondsSince(Clock::time_point pStart)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - pStart).count();
}

int wrapWest(int pWest)
{
    return pWest < -180 ? pWest + 360 : (pWest > 179 ? pWest - 360 : pWest);
}

// Onbellek anahtari: karonun butun aday DSF'leri (yol, boyut, degisiklik zamani)
std::string cacheKey(const std::vector<DsfCandidate>& pCandidates)
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
bool hasNoBaseMarker(const std::string& pPath, const std::string& pKey)
{
    std::ifstream tFile(pPath, std::ios::binary);
    if (!tFile)
    {
        return false;
    }
    const std::string tKey((std::istreambuf_iterator<char>(tFile)), std::istreambuf_iterator<char>());
    return tKey == pKey;
}

void writeNoBaseMarker(const std::string& pPath, const std::string& pKey)
{
    writeFileAtomic(pPath,
                    [&pKey](std::ofstream& tFile)
                    {
                        tFile << pKey;
                    });
}

// istegin(karo / parca) ortasinin odaga uzakligi; tanimi asagida (pickRequest'in yaninda)
double focusDistance(const TerrainImageKey& pKey, double pFocusLat, double pFocusLon);

bool readWholeFile(const std::string& pPath, std::vector<uint8_t>* pOut)
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

int TerrainStore::imageSize(int pLevel)
{
    const int kSizes[kLevelCount] = {64, 256, 1024, 1024, 1024, 1024};
    return kSizes[pLevel];
}

int TerrainStore::chunksPerSide(int pLevel)
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
    stop();
}

void TerrainStore::start(const std::string& pXPlaneDirectory, const std::string& pCacheDirectory,
                         const TerrainPalette& pPalette, const std::vector<TerrainImageStyle>& pStyles)
{
    if (mThread.joinable())
    {
        return;
    }
    mXPlaneDirectory = pXPlaneDirectory;
    mCacheDirectory = pCacheDirectory;
    {
        // Palet 0 = verilen palet. Start'tan once addPalette ile eklenenler (1, 2 ...) korunur.
        std::lock_guard<std::mutex> tLock(mMutex);
        if (mPalettes.empty())
        {
            mPalettes.push_back(pPalette);
        }
        else
        {
            mPalettes[0] = pPalette;
        }
    }
    mStyles = pStyles;
    mStopping = false;
    mThread = std::thread(
        [this]()
        {
            workerLoop();
        });
#if defined(_WIN32)
    // Dusuk oncelik: isci X-Plane'in kare suresinden pay almasin (ekran istekleri yine once gelir)
    SetThreadPriority(static_cast<HANDLE>(mThread.native_handle()), THREAD_PRIORITY_BELOW_NORMAL);
#endif
}

void TerrainStore::setCacheLimit(uint64_t pBytes)
{
    if (mCacheLimit.exchange(pBytes) != pBytes)
    {
        mTrimWanted.store(true);  // isci bos aninda yeni sinira gore kirpar
        mWake.notify_one();
    }
}

void TerrainStore::setMemoryScale(double pScale)
{
    mMemoryScale.store(std::max(1.0, pScale));
}

size_t TerrainStore::scaledLimit(size_t pBase) const
{
    return static_cast<size_t>(std::lround(static_cast<double>(pBase) * mMemoryScale.load()));
}

int TerrainStore::addPalette(const TerrainPalette& pPalette)
{
    std::lock_guard<std::mutex> tLock(mMutex);
    mPalettes.push_back(pPalette);  // ilk eklenen 0 numarayi alir (Start ayni paleti 0'a yazar)
    return static_cast<int>(mPalettes.size()) - 1;
}

void TerrainStore::trimCache()
{
    // ISCI thread'i: dosyalari sadece isci okur/yazar, silmek guvenli
    mCacheGrew = false;
    const uint64_t tLimit = mCacheLimit.load();
    if (mCacheDirectory.empty() || tLimit == 0)
    {
        return;
    }
    const CacheTrimResult tResult = trimTileCache(mCacheDirectory, tLimit);
    if (tResult.mRemovedTiles > 0)
    {
        addLog(std::format("terrain cache: {:.0f} MB over the {:.0f} MB limit, removed {} least recently used tiles "
                           "({:.0f} MB)",
                           tResult.mTotalBytes / 1048576.0, tLimit / 1048576.0, tResult.mRemovedTiles,
                           tResult.mRemovedBytes / 1048576.0));
    }
}

void TerrainStore::setFocus(double pLat, double pLon)
{
    std::lock_guard<std::mutex> tLock(mMutex);
    mFocusLat = pLat;
    mFocusLon = pLon;
}

void TerrainStore::setPrefetchTiles(const std::vector<int>& pTiles)
{
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        mPrefetch.assign(pTiles.begin(), pTiles.end());
    }
    mWake.notify_one();
}

void TerrainStore::startPrecache(const std::vector<int>& pTiles)
{
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        mPrecache.assign(pTiles.begin(), pTiles.end());
        mPrecacheDone = 0;
        mPrecacheTotal = static_cast<int>(pTiles.size());
    }
    mWake.notify_one();
}

void TerrainStore::cancelPrecache()
{
    std::lock_guard<std::mutex> tLock(mMutex);
    mPrecache.clear();
    mPrecacheTotal = mPrecacheDone;  // bitmis sayilir
}

bool TerrainStore::precacheProgress(int* pDone, int* pTotal) const
{
    std::lock_guard<std::mutex> tLock(mMutex);
    *pDone = mPrecacheDone;
    *pTotal = mPrecacheTotal;
    return mPrecacheDone < mPrecacheTotal;
}

bool TerrainStore::serveBackgroundTile()
{
    // ISCI thread'i. Ekranin istegi varken calismaz (dusuk oncelik): bir karo ~0,5-1 s surer.
    int tKey = -1;
    bool tPrecache = false;
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        if (!mRequests.empty() || !mVectorRequests.empty())
        {
            return false;
        }
        while (tKey < 0 && (!mPrecache.empty() || !mPrefetch.empty()))
        {
            tPrecache = !mPrecache.empty();
            std::deque<int>& tQueue = tPrecache ? mPrecache : mPrefetch;
            const int tCandidate = tQueue.front();
            tQueue.pop_front();
            if (mBackgroundDone.count(tCandidate) == 0)
            {
                tKey = tCandidate;
            }
            else if (tPrecache)
            {
                ++mPrecacheDone;  // bu oturumda zaten hazirlandi
            }
        }
        if (tKey < 0)
        {
            return false;
        }
    }
    const int tWest = tKey % 360 - 180;
    const int tSouth = tKey / 360 - 90;
    bool tInMemory = false;
    for (const auto& tEntry : mRasters)
    {
        tInMemory = tInMemory || tEntry.first == tKey;
    }
    if (!tInMemory)
    {
        // Gecici nesnelere: onbellek dosyalari yazilir (ya da guncelse sadece okunup dokunulur),
        // bellekteki raster / rakim LRU'su degismez (ekrandaki karolar itilmez)
        TerrainRaster tRaster;
        ElevationGrid tElevation;
        TerrainVectors tVectors;
        std::string tLog;
        TerrainTileState tState = TerrainTileState::Unknown;
        const Clock::time_point tBackgroundStart = Clock::now();
        buildRaster(mIndex, tWest, tSouth, mCacheDirectory, &tRaster, &tLog, &tState, &tElevation, &tVectors);
        const double tBackgroundMs = millisecondsSince(tBackgroundStart);
        mStats.mBackgroundMs += tBackgroundMs;
        ++mStats.mBackgroundCount;
        {
            std::lock_guard<std::mutex> tLock(mMutex);
            if (!mRequests.empty())
            {
                ++mStats.mBlockedCount;  // bu sirada ekran istegi geldi: en cok bu kadar bekledi
                mStats.mBlockedMs += tBackgroundMs;
            }
        }
        if (!tLog.empty())
        {
            tLog.pop_back();
            addLog((tPrecache ? "pre-cache: " : "prefetch: ") + tLog);
        }
        setState(tWest, tSouth, tState);
        mCacheGrew = true;
    }
    std::lock_guard<std::mutex> tLock(mMutex);
    mBackgroundDone.insert(tKey);
    if (tPrecache)
    {
        ++mPrecacheDone;
    }
    return true;
}

void TerrainStore::stop()
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

bool TerrainStore::isIndexReady() const
{
    return mIndexReady.load();
}

TileCoverage TerrainStore::coverageOf(int pWest, int pSouth) const
{
    if (!mIndexReady.load())
    {
        return TileCoverage::NoData;
    }
    return mIndex.coverageOf(pWest, pSouth);  // dizin hazir olduktan sonra degismez
}

TerrainTileState TerrainStore::stateOf(int pWest, int pSouth) const
{
    std::lock_guard<std::mutex> tLock(mMutex);
    const auto tFound = mStates.find(SceneryIndex::tileKey(pWest, pSouth));
    return tFound == mStates.end() ? TerrainTileState::Unknown : tFound->second;
}

void TerrainStore::beginFrame()
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
        const auto [tEntry, tInserted] = mRequests.try_emplace(pKey);
        tEntry->second.mFrame = mFrame;
        // Uzaklik istek aninda: isteyen haritanin merkezi (setFocus, o haritanin Draw'inda) gecerli
        tEntry->second.mDistance = focusDistance(pKey, mFocusLat, mFocusLon);
        if (tInserted)
        {
            tEntry->second.mFirst = Clock::now();
        }
    }
    mWake.notify_one();
}

void TerrainStore::setDiagnostics(bool pOn)
{
    mDiagnostics.store(pOn);
}

bool TerrainStore::hasElevation(int pWest, int pSouth) const
{
    std::lock_guard<std::mutex> tLock(mMutex);
    return mElevationReady.count(SceneryIndex::tileKey(pWest, pSouth)) > 0;
}

bool TerrainStore::popImage(TerrainImage* pOut)
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

bool TerrainStore::isIdle() const
{
    std::lock_guard<std::mutex> tLock(mMutex);
    // Uretilmekte olan goruntu de is sayilir: katman ayni istegi tekrarlasa bile kuyruga girmez (Request)
    return mRequests.empty() && mResults.empty() && mVectorRequests.empty() && mVectorResults.empty() &&
           !mHasInFlight && mStatus.empty() && mIndexReady.load();
}

void TerrainStore::requestVectors(int pWest, int pSouth)
{
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        const int tKey = SceneryIndex::tileKey(pWest, pSouth);
        if (mVectorsServed.count(tKey))
        {
            return;  // verildi; katman kendi kopyasini tutar (atinca ForgetVectors ile yeniden ister)
        }
        mVectorRequests[tKey] = mFrame;
    }
    mWake.notify_one();
}

bool TerrainStore::popVectors(std::shared_ptr<const TerrainVectors>* pOut)
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

std::string TerrainStore::statusText() const
{
    std::lock_guard<std::mutex> tLock(mMutex);
    return mStatus;
}

std::string TerrainStore::takeLogLines()
{
    std::lock_guard<std::mutex> tLock(mMutex);
    std::string tLines;
    tLines.swap(mLog);
    return tLines;
}

void TerrainStore::addLog(const std::string& pLine)
{
    std::lock_guard<std::mutex> tLock(mMutex);
    mLog += pLine + "\n";
}

void TerrainStore::setState(int pWest, int pSouth, TerrainTileState pState)
{
    std::lock_guard<std::mutex> tLock(mMutex);
    mStates[SceneryIndex::tileKey(pWest, pSouth)] = pState;
}

namespace
{

// istegin(karo / parca) ortasinin odaga uzakligi (derece^2, boylam enleme gore kisalir)
double focusDistance(const TerrainImageKey& pKey, double pFocusLat, double pFocusLon)
{
    const int tCount = TerrainStore::chunksPerSide(pKey.mLevel);
    const int tColumn = tCount > 1 ? pKey.mChunk % tCount : 0;
    const int tRow = tCount > 1 ? pKey.mChunk / tCount : 0;
    const double tLon = pKey.mWest + (tColumn + 0.5) / tCount;
    const double tLat = pKey.mSouth + 1.0 - (tRow + 0.5) / tCount;  // satir 0 = kuzey
    double tDeltaLon = std::abs(tLon - pFocusLon);
    tDeltaLon = std::min(tDeltaLon, 360.0 - tDeltaLon);
    const double tCos = std::cos(pFocusLat * 3.14159265358979 / 180.0);
    const double tDeltaLat = tLat - pFocusLat;
    return tDeltaLon * tDeltaLon * tCos * tCos + tDeltaLat * tDeltaLat;
}

}  // namespace

bool TerrainStore::pickRequest(TerrainImageKey* pOut)
{
    // mMutex kilitliyken cagrilir. En son istenen; esitlikte kaba seviye once (once bulanik ama
    // hizli bir goruntu, sonra keskini); o da esitse ekran merkezine yakin olan.
    bool tFound = false;
    uint64_t tBestFrame = 0;
    int tBestLevel = 0;
    double tBestDistance = 0.0;
    for (auto tIterator = mRequests.begin(); tIterator != mRequests.end();)
    {
        if (tIterator->second.mFrame + kStaleFrames < mFrame)
        {
            tIterator = mRequests.erase(tIterator);  // ekrandan cikmis
            continue;
        }
        const uint64_t tFrame = tIterator->second.mFrame;
        const int tLevel = tIterator->first.mLevel;
        const double tDistance = tIterator->second.mDistance;
        const bool tBetter = !tFound || tFrame > tBestFrame ||
                             (tFrame == tBestFrame && (tLevel < tBestLevel ||
                                                       (tLevel == tBestLevel && tDistance < tBestDistance)));
        if (tBetter)
        {
            tFound = true;
            tBestFrame = tFrame;
            tBestLevel = tLevel;
            tBestDistance = tDistance;
            *pOut = tIterator->first;
        }
        ++tIterator;
    }
    if (tFound)
    {
        mPickedFirst = mRequests[*pOut].mFirst;
        mRequests.erase(*pOut);
    }
    return tFound;
}

bool TerrainStore::buildRaster(const SceneryIndex& pIndex, int pWest, int pSouth, const std::string& pCacheDirectory,
                               TerrainRaster* pOut, std::string* pLog, TerrainTileState* pState,
                               ElevationGrid* pElevation, TerrainVectors* pVectors)
{
    // Temel mesh yoksa: deniz ya da veri yok (SceneryIndex: dunya tablosu, yoksa 10 derece klasor kurali)
    const TerrainTileState tNoBaseState = pIndex.noBaseCoverage(pWest, pSouth) == TileCoverage::Ocean
                                              ? TerrainTileState::Ocean
                                              : TerrainTileState::NoData;
    const std::vector<DsfCandidate>* tCandidates = pIndex.candidatesFor(pWest, pSouth);
    if (!tCandidates)
    {
        *pState = tNoBaseState;
        return false;
    }
    const std::string tName = SceneryIndex::tileName(pWest, pSouth);
    const std::string tCachePath = pCacheDirectory + tName + ".terrain";
    const std::string tElevationPath = pCacheDirectory + tName + ".elevation";
    const std::string tVectorsPath = pCacheDirectory + tName + ".vectors";
    const std::string tNoBasePath = pCacheDirectory + tName + ".nobase";
    const std::string tKey = cacheKey(*tCandidates);
    auto tLoadRaster = [&]()
    {
        TerrainProfile::ScopedStage tStage(TerrainProfile::Stage::loadRaster);
        return pOut->load(tCachePath, tKey);
    };
    auto tLoadElevation = [&]()
    {
        TerrainProfile::ScopedStage tStage(TerrainProfile::Stage::loadElevation);
        return !pElevation || pElevation->load(tElevationPath, tKey);
    };
    auto tLoadVectors = [&]()
    {
        TerrainProfile::ScopedStage tStage(TerrainProfile::Stage::loadVectors);
        return !pVectors || pVectors->load(tVectorsPath, tKey);
    };
    if (!pCacheDirectory.empty() && tLoadRaster() && tLoadElevation() && tLoadVectors())
    {
        *pState = TerrainTileState::Ready;
        touchCacheFile(tCachePath);  // en son kullanilan (disk siniri: CacheTrim)
        return true;  // onbellek guncel: DSF'e hic dokunulmadi
    }
    if (!pCacheDirectory.empty() && hasNoBaseMarker(tNoBasePath, tKey))
    {
        *pState = tNoBaseState;  // daha once bakildi: bu karoda sadece overlay var
        return false;
    }

    const Clock::time_point tStart = Clock::now();
    std::vector<uint8_t> tData;
    DsfReader tReader;
    std::string tPath;
    if (!openBaseMesh(*tCandidates, tName, &tData, &tReader, &tPath, pLog))
    {
        // Sadece overlay DSF'leri var (veya hepsi okunamadi): temel mesh yok
        *pState = tNoBaseState;
        if (!pCacheDirectory.empty())
        {
            writeNoBaseMarker(tNoBasePath, tKey);
        }
        *pLog += "terrain " + tName + ": no base mesh (only overlays)\n";
        return false;
    }

    // Arazi turu indeksi siniflara cevrilir
    std::vector<TerrainClass> tClasses;
    for (const std::string& tTerrain : tReader.info().mTerrainNames)
    {
        tClasses.push_back(classifyTerrain(tTerrain));
    }
    pOut->reset(pWest, pSouth);
    std::string tError;
    std::optional<TerrainProfile::ScopedStage> tTriangleStage(TerrainProfile::Stage::DsfTriangles);
    const bool tOk = tReader.forEachTriangle(
        [&](const DsfTriangle& pTriangle)
        {
            if ((pTriangle.mPatchFlags & DsfReader::kPatchFlagOverlay) == 0 && pTriangle.mTerrain >= 0 &&
                pTriangle.mTerrain < static_cast<int>(tClasses.size()))
            {
                pOut->fillTriangle(pTriangle.mLon, pTriangle.mLat, tClasses[pTriangle.mTerrain]);
            }
        },
        &tError);
    if (!tOk)
    {
        *pLog += "terrain " + tName + ": " + tError + " (" + tPath + ")\n";
        *pState = TerrainTileState::Failed;
        return false;
    }
    tTriangleStage.reset();
    // vektorler: temel mesh'in poligon / zincirleri + overlay DSF'lerinin dislama alanlari
    std::optional<TerrainProfile::ScopedStage> tVectorStage(TerrainProfile::Stage::DsfVectors);
    TerrainVectors tLocalVectors;
    TerrainVectors& tVectors = pVectors ? *pVectors : tLocalVectors;
    tVectors.reset(pWest, pSouth);
    std::string tVectorError;
    if (!tVectors.addFromDsf(tReader, &tVectorError))
    {
        *pLog += "terrain " + tName + ": vectors skipped: " + tVectorError + "\n";
        tVectors.reset(pWest, pSouth);
    }
    tVectorStage.emplace(TerrainProfile::Stage::Overlays);
    for (const DsfCandidate& tCandidate : *tCandidates)
    {
        if (tCandidate.mPath == tPath)
        {
            continue;  // temel mesh
        }
        std::vector<uint8_t> tOverlayData;
        DsfReader tOverlay;
        std::string tOverlayError;
        if (!readWholeFile(tCandidate.mPath, &tOverlayData))
        {
            continue;
        }
        if (SevenZip::isArchive(tOverlayData.data(), tOverlayData.size()))
        {
            std::vector<uint8_t> tPlain;
            if (!SevenZip::extractFirstFile(tOverlayData, &tPlain, &tOverlayError))
            {
                continue;
            }
            tOverlayData.swap(tPlain);
        }
        if (tOverlay.open(tOverlayData, &tOverlayError) && tOverlay.info().mOverlay)
        {
            tVectors.addExclusions(tOverlay.info());
        }
    }
    tVectors.applyNetworkExclusions();
    tVectorStage.emplace(TerrainProfile::Stage::FillAreas);
    pOut->fillAreas(tVectors);
    tVectorStage.emplace(TerrainProfile::Stage::CacheSave);
    if (!pCacheDirectory.empty() && !tVectors.save(tVectorsPath, tKey))
    {
        *pLog += "terrain " + tName + ": cannot write cache " + tVectorsPath + "\n";
    }

    if (!pCacheDirectory.empty() && !pOut->save(tCachePath, tKey))
    {
        *pLog += "terrain " + tName + ": cannot write cache " + tCachePath + "\n";
    }

    tVectorStage.reset();
    // Rakim: tam cozunurluklu DEM + sea_level ayri dosyaya (DEM yoksa bos dosya: kabartmasiz karo)
    std::string tElevationText = "no elevation";
    if (pElevation)
    {
        DsfRaster tDem;
        DsfRaster tSeaLevel;
        bool tHasSea = false;
        {
            TerrainProfile::ScopedStage tStage(TerrainProfile::Stage::ElevationBuild);
            const bool tHasDem = tReader.readRaster("elevation", &tDem);
            tHasSea = tReader.readRaster("sea_level", &tSeaLevel);
            if (!tHasDem || !pElevation->build(tDem, tHasSea ? &tSeaLevel : nullptr))
            {
                *pElevation = ElevationGrid();
            }
        }
        const Clock::time_point tPackStart = Clock::now();
        {
            TerrainProfile::ScopedStage tStage(TerrainProfile::Stage::ElevationPack);
            if (!pCacheDirectory.empty() && !pElevation->save(tElevationPath, tKey))
            {
                *pLog += "terrain " + tName + ": cannot write cache " + tElevationPath + "\n";
            }
        }
        if (!pElevation->isEmpty())
        {
            std::error_code tSizeError;
            const uintmax_t tBytes = std::filesystem::file_size(tElevationPath, tSizeError);
            tElevationText = std::format("elevation {} posts, sea_level {}, packed {:.2f} MB in {:.0f} ms",
                                         pElevation->size(0), tHasSea ? "yes" : "no",
                                         tSizeError ? 0.0 : tBytes / 1048576.0, millisecondsSince(tPackStart));
        }
    }
    *pLog += std::format("terrain {}: built from {} in {:.1f} s (water {:.0f}%, {}, vectors {} areas {} lines)\n",
                         tName, tPath, std::chrono::duration<double>(Clock::now() - tStart).count(),
                         100.0 * pOut->countOf(TerrainClass::Water) / (TerrainRaster::kSize * TerrainRaster::kSize),
                         tElevationText, tVectors.areas().size(), tVectors.lines().size());
    *pState = TerrainTileState::Ready;
    return true;
}

bool TerrainStore::loadElevation(const SceneryIndex& pIndex, int pWest, int pSouth,
                                 const std::string& pCacheDirectory, ElevationGrid* pOut)
{
    const std::vector<DsfCandidate>* tCandidates = pIndex.candidatesFor(pWest, pSouth);
    if (!tCandidates || pCacheDirectory.empty())
    {
        return false;
    }
    const std::string tPath = pCacheDirectory + SceneryIndex::tileName(pWest, pSouth) + ".elevation";
    return pOut->load(tPath, cacheKey(*tCandidates));
}

bool TerrainStore::openBaseMesh(const std::vector<DsfCandidate>& pCandidates, const std::string& pName,
                                std::vector<uint8_t>* pData, DsfReader* pReader, std::string* pPath,
                                std::string* pLog)
{
    // Oncelik sirasiyla: overlay olmayan ilk DSF karonun temel mesh'idir
    TerrainProfile::ScopedStage tStage(TerrainProfile::Stage::DsfUnpack);  // okuma + 7z acma + baslik
    for (const DsfCandidate& tCandidate : pCandidates)
    {
        if (!readWholeFile(tCandidate.mPath, pData))
        {
            *pLog += "terrain " + pName + ": cannot read " + tCandidate.mPath + "\n";
            continue;
        }
        std::string tError;
        if (SevenZip::isArchive(pData->data(), pData->size()))
        {
            std::vector<uint8_t> tPlain;
            if (!SevenZip::extractFirstFile(*pData, &tPlain, &tError))
            {
                *pLog += "terrain " + pName + ": " + tError + " (" + tCandidate.mPath + ")\n";
                continue;
            }
            pData->swap(tPlain);
        }
        if (!pReader->open(*pData, &tError))
        {
            *pLog += "terrain " + pName + ": " + tError + " (" + tCandidate.mPath + ")\n";
            continue;
        }
        if (pReader->info().mOverlay)
        {
            continue;  // ustune serilen katman (havalimani, nesneler): zemin degil
        }
        *pPath = tCandidate.mPath;
        return true;
    }
    return false;
}

bool TerrainStore::loadTriangles(const SceneryIndex& pIndex, int pWest, int pSouth, TerrainTriangles* pOut,
                                 std::string* pLog)
{
    const std::vector<DsfCandidate>* tCandidates = pIndex.candidatesFor(pWest, pSouth);
    if (!tCandidates)
    {
        return false;
    }
    const std::string tName = SceneryIndex::tileName(pWest, pSouth);
    const Clock::time_point tStart = Clock::now();
    std::vector<uint8_t> tData;
    DsfReader tReader;
    std::string tPath;
    if (!openBaseMesh(*tCandidates, tName, &tData, &tReader, &tPath, pLog))
    {
        return false;
    }
    std::vector<TerrainClass> tClasses;
    for (const std::string& tTerrain : tReader.info().mTerrainNames)
    {
        tClasses.push_back(classifyTerrain(tTerrain));
    }
    pOut->reset(pWest, pSouth);
    std::string tError;
    const bool tOk = tReader.forEachTriangle(
        [&](const DsfTriangle& pTriangle)
        {
            if ((pTriangle.mPatchFlags & DsfReader::kPatchFlagOverlay) == 0 && pTriangle.mTerrain >= 0 &&
                pTriangle.mTerrain < static_cast<int>(tClasses.size()))
            {
                pOut->add(pTriangle.mLon, pTriangle.mLat, tClasses[pTriangle.mTerrain]);
            }
        },
        &tError);
    if (!tOk)
    {
        *pLog += "terrain detail " + tName + ": " + tError + " (" + tPath + ")\n";
        return false;
    }
    *pLog += std::format("terrain detail {}: {} triangles, {:.0f} MB in {:.1f} s\n", tName, pOut->triangleCount(),
                         pOut->memoryBytes() / 1048576.0,
                         std::chrono::duration<double>(Clock::now() - tStart).count());
    return true;
}

TerrainTriangles* TerrainStore::trianglesFor(int pWest, int pSouth)
{
    const int tKey = SceneryIndex::tileKey(pWest, pSouth);
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
        mStatus = "Loading terrain detail " + SceneryIndex::tileName(pWest, pSouth);
    }
    std::unique_ptr<TerrainTriangles> tTriangles = std::make_unique<TerrainTriangles>();
    std::string tLog;
    const Clock::time_point tTriangleStart = Clock::now();
    const bool tOk = loadTriangles(mIndex, pWest, pSouth, tTriangles.get(), &tLog);
    ++mStats.mTriangleLoads[tKey];
    mStats.mTriangleMs += millisecondsSince(tTriangleStart);
    if (!tLog.empty())
    {
        tLog.pop_back();
        addLog(tLog);
    }
    if (!tOk)
    {
        return nullptr;
    }
    mTriangles.emplace_front(tKey, std::move(tTriangles));
    while (mTriangles.size() > scaledLimit(kMaxTriangleTiles))
    {
        mTriangles.pop_back();
    }
    return mTriangles.front().second.get();
}

TerrainRaster* TerrainStore::rasterFor(int pWest, int pSouth)
{
    const int tKey = SceneryIndex::tileKey(pWest, pSouth);
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
        mStatus = "Building terrain " + SceneryIndex::tileName(pWest, pSouth);
    }
    std::unique_ptr<TerrainRaster> tRaster = std::make_unique<TerrainRaster>();
    std::unique_ptr<ElevationGrid> tElevation = std::make_unique<ElevationGrid>();
    std::shared_ptr<TerrainVectors> tVectors = std::make_shared<TerrainVectors>();
    std::string tLog;
    TerrainTileState tState = TerrainTileState::Unknown;
    const Clock::time_point tMissStart = Clock::now();
    const bool tOk = buildRaster(mIndex, pWest, pSouth, mCacheDirectory, tRaster.get(), &tLog, &tState,
                                 tElevation.get(), tVectors.get());
    ++mStats.mRasterMisses[tKey];
    mStats.mRasterMissMs += millisecondsSince(tMissStart);
    if (!tLog.empty())
    {
        tLog.pop_back();
        addLog(tLog);
    }
    setState(pWest, pSouth, tState);
    if (!tOk)
    {
        return nullptr;
    }
    mCacheGrew = true;  // (onbellekten okunduysa da zarari yok: kirpma ucuz)
    keepElevation(pWest, pSouth, std::move(tElevation));
    mVectors.emplace_front(tKey, std::move(tVectors));
    while (mVectors.size() > scaledLimit(kMaxVectorTiles))
    {
        mVectors.pop_back();
    }
    mRasters.emplace_front(tKey, std::move(tRaster));
    while (mRasters.size() > scaledLimit(kMaxRastersInMemory))
    {
        mRasters.pop_back();
    }
    return mRasters.front().second.get();
}

void TerrainStore::keepElevation(int pWest, int pSouth, std::unique_ptr<ElevationGrid> pGrid)
{
    const int tKey = SceneryIndex::tileKey(pWest, pSouth);
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
    if (mElevations.size() > scaledLimit(kMaxElevationTiles))
    {
        // En eski tam izgara silinmez: kucultulup kaba listeye gecer (uzak zoom'da yeniden diskten okunmasin)
        std::pair<int, std::unique_ptr<ElevationGrid>> tOldest = std::move(mElevations.back());
        mElevations.pop_back();
        tOldest.second->dropFineLevels(kCoarseElevationLevel);
        keepCoarseElevation(tOldest.first, std::move(tOldest.second));
    }
    std::lock_guard<std::mutex> tLock(mMutex);
    mElevationReady.insert(tKey);
}

void TerrainStore::keepCoarseElevation(int pKey, std::unique_ptr<ElevationGrid> pGrid)
{
    mCoarseElevations.emplace_front(pKey, std::move(pGrid));
    while (mCoarseElevations.size() > scaledLimit(kMaxCoarseElevationTiles))
    {
        mCoarseElevations.pop_back();
    }
    std::lock_guard<std::mutex> tLock(mMutex);
    mElevationReady.insert(pKey);
}

const ElevationGrid* TerrainStore::elevationFor(int pWest, int pSouth, int pLevel)
{
    const int tKey = SceneryIndex::tileKey(pWest, pSouth);
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
    {
        TerrainProfile::ScopedStage tStage(TerrainProfile::Stage::loadElevation);
        if (!loadElevation(mIndex, pWest, pSouth, mCacheDirectory, tGrid.get()))
        {
            return nullptr;
        }
    }
    ++mStats.mElevationLoads;
    addLog(std::format("terrain {}: elevation from cache in {:.0f} ms ({})", SceneryIndex::tileName(pWest, pSouth),
                       millisecondsSince(tStart), tCoarseIsEnough ? "coarse" : "full"));
    if (tCoarseIsEnough)
    {
        tGrid->dropFineLevels(kCoarseElevationLevel);  // uzak zoom: tam izgara listesini doldurmasin
        keepCoarseElevation(tKey, std::move(tGrid));
        return mCoarseElevations.front().second.get();
    }
    keepElevation(pWest, pSouth, std::move(tGrid));
    return mElevations.front().second.get();
}

void TerrainStore::buildImageFor(const TerrainImageKey& pKey, const TerrainRaster& pRaster, TerrainImage* pOut)
{
    // Rakim: karo ve 8 komsusu. Komsu sadece goruntu o kenara degiyorsa gerekir.
    const int tChunks = chunksPerSide(pKey.mLevel);
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
    const int tLevel = ElevationGrid::nominalLevelFor(tMetersPerPixel);
    ElevationTiles tTiles;
    tTiles.mTiles[1][1] = elevationFor(pKey.mWest, pKey.mSouth, tLevel);
    for (int tRow = 0; tRow < 3 && tTiles.mTiles[1][1]; ++tRow)
    {
        for (int tColumn = 0; tColumn < 3; ++tColumn)
        {
            const int tWest = wrapWest(pKey.mWest + tColumn - 1);
            const int tSouth = pKey.mSouth + 1 - tRow;  // satir 0 = kuzey komsu
            if ((tRow == 1 && tColumn == 1) || !tTouches[tRow][tColumn] || tSouth < -90 || tSouth > 89 ||
                mIndex.coverageOf(tWest, tSouth) != TileCoverage::hasDsf)
            {
                continue;  // gerekmiyor ya da hic olmayacak (deniz / veri yok): tek tarafli fark kalici
            }
            tTiles.mTiles[tRow][tColumn] = elevationFor(tWest, tSouth, tLevel);
            // Eksik say: sadece henuz hic hazir olmamis komsu. Bir kez hazir olup simdi bulunamayan
            // (bellekten atilmis, onbellege yazilamamis) komsu beklenmez: yoksa katman bu goruntuyu
            // her karede yeniden ister, sonsuz yeniden uretim olur.
            if (!tTiles.mTiles[tRow][tColumn] && !hasElevation(tWest, tSouth))
            {
                pOut->mMissingNeighbors.push_back(SceneryIndex::tileKey(tWest, tSouth));
            }
        }
    }
    // Isaretciler bu goruntu boyunca gecerli kalir: tam liste 12 >= 9; tam listeden atilan izgara
    // silinmez, kaba listeye tasinir (ayni nesne, sadece ince seviyeleri birakilir; bu goruntu
    // kaba seviye okuyorsa sorun yok, ince seviye okuyorsa 9 karo zaten tam listede sigar)
    const ElevationTiles* tElevation = tTiles.mTiles[1][1] ? &tTiles : nullptr;

    // Bu goruntunun paleti (harita basina olabilir): kilit altinda kopyala, uretim kilitsiz surer
    TerrainPalette tPalette;
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        const size_t tIndex = pKey.mPalette >= 0 && pKey.mPalette < static_cast<int>(mPalettes.size())
                                  ? static_cast<size_t>(pKey.mPalette)
                                  : 0;
        tPalette = mPalettes[tIndex];
    }
    const TerrainImageStyle tStyle = pKey.mStyle >= 0 && pKey.mStyle < static_cast<int>(mStyles.size())
                                         ? mStyles[pKey.mStyle]
                                         : TerrainImageStyle();
    if (pKey.mLevel >= kDetailLevel)
    {
        // Yakin zoom: parca raster'dan degil, dogrudan DSF ucgenlerinden boyanir
        const TerrainTriangles* tTriangles = trianglesFor(pKey.mWest, pKey.mSouth);
        if (!tTriangles)
        {
            pOut->mRgba.clear();
            return;
        }
        const std::shared_ptr<TerrainVectors> tVectors =
            pKey.mAreas != 0 ? vectorsFor(pKey.mWest, pKey.mSouth) : nullptr;
        pRaster.buildDetailImage(*tTriangles, tChunks, pKey.mChunk, pOut->mSize, tPalette, tStyle, &pOut->mRgba,
                                 tElevation, tVectors.get(), pKey.mAreas);
        return;
    }
    const int tSpan = TerrainRaster::kSize / tChunks;
    const int tX0 = tChunkColumn * tSpan;
    const int tY0 = tChunkRow * tSpan;
    pRaster.buildImage(tX0, tY0, tX0 + tSpan, tY0 + tSpan, pOut->mSize, tPalette, tStyle, &pOut->mRgba, tElevation,
                       pKey.mAreas);
}

std::shared_ptr<TerrainVectors> TerrainStore::vectorsFor(int pWest, int pSouth)
{
    const int tKey = SceneryIndex::tileKey(pWest, pSouth);
    for (auto tIterator = mVectors.begin(); tIterator != mVectors.end(); ++tIterator)
    {
        if (tIterator->first == tKey)
        {
            mVectors.splice(mVectors.begin(), mVectors, tIterator);
            return mVectors.front().second;
        }
    }
    // onbellekten(raster ile her zaman birlikte yazilir). Raster bellekten SILINMEZ: cagiran (yakin zoom
    // goruntusu) o anda ona basvuruyor olabilir.
    std::shared_ptr<TerrainVectors> tVectors = std::make_shared<TerrainVectors>();
    ++mStats.mVectorMisses[tKey];
    if (!loadVectors(mIndex, pWest, pSouth, mCacheDirectory, tVectors.get()))
    {
        // Onbellek yoksa karo henuz uretilmemistir: uretilirken (RasterFor) vektorler de gelir
        if (!rasterFor(pWest, pSouth) || mVectors.empty() || mVectors.front().first != tKey)
        {
            return nullptr;
        }
        return mVectors.front().second;
    }
    mVectors.emplace_front(tKey, tVectors);
    while (mVectors.size() > scaledLimit(kMaxVectorTiles))
    {
        mVectors.pop_back();
    }
    return tVectors;
}

bool TerrainStore::loadVectors(const SceneryIndex& pIndex, int pWest, int pSouth, const std::string& pCacheDirectory,
                               TerrainVectors* pOut)
{
    const std::vector<DsfCandidate>* tCandidates = pIndex.candidatesFor(pWest, pSouth);
    if (!tCandidates || pCacheDirectory.empty())
    {
        return false;
    }
    const std::string tPath = pCacheDirectory + SceneryIndex::tileName(pWest, pSouth) + ".vectors";
    return pOut->load(tPath, cacheKey(*tCandidates));
}

void TerrainStore::forgetVectors(int pWest, int pSouth)
{
    std::lock_guard<std::mutex> tLock(mMutex);
    mVectorsServed.erase(SceneryIndex::tileKey(pWest, pSouth));
}

bool TerrainStore::serveVectorRequest()
{
    int tKey = 0;
    {
        std::lock_guard<std::mutex> tLock(mMutex);
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
    const std::shared_ptr<TerrainVectors> tVectors = vectorsFor(tKey % 360 - 180, tKey / 360 - 90);
    if (tVectors)
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        mVectorResults.push_back(tVectors);
    }
    return true;
}

void TerrainStore::noteImageTime(int pLevel, double pMilliseconds)
{
    if (pLevel >= 0 && pLevel < kLevelCount)
    {
        ++mImageCounts[pLevel];
        mImageMilliseconds[pLevel] += pMilliseconds;
    }
}

void TerrainStore::logImageTimes()
{
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
        addLog("terrain images (count x average):" + tText);
    }
    const std::string tStages = TerrainProfile::takeReport();
    WorkStats tStats;
    std::swap(tStats, mStats);
    if (!mDiagnostics.load())
    {
        return;
    }
    if (!tStages.empty())
    {
        addLog("terrain profile stages (count x avg ms = total):" + tStages);
    }
    auto tTiles = [](const std::map<int, int>& pCounts)
    {
        std::string tList;
        int tTotal = 0;
        for (const auto& [tKey, tCount] : pCounts)
        {
            tTotal += tCount;
            tList += std::format(" {}x{}", SceneryIndex::tileName(tKey % 360 - 180, tKey / 360 - 90), tCount);
        }
        return std::format("{}{}", tTotal, tList);
    };
    addLog(std::format("terrain profile reloads: raster {} ({:.0f} ms); triangles {} ({:.0f} ms); vectors {}; "
                       "elevation from disk {}",
                       tTiles(tStats.mRasterMisses), tStats.mRasterMissMs, tTiles(tStats.mTriangleLoads),
                       tStats.mTriangleMs, tTiles(tStats.mVectorMisses), tStats.mElevationLoads));
    std::string tWait;
    for (int tLevel = 0; tLevel < kLevelCount; ++tLevel)
    {
        const int tCount = tStats.mLatencyCount[tLevel];
        if (tCount > 0)
        {
            tWait += std::format(" L{} {}: queue {:.0f} ready {:.0f} max {:.0f} ms;", tLevel, tCount,
                                 tStats.mQueueMs[tLevel] / tCount, tStats.mReadyMs[tLevel] / tCount,
                                 tStats.mReadyMaxMs[tLevel]);
        }
    }
    addLog(std::format("terrain profile waits (avg from first request):{} background tiles {} ({:.0f} ms), "
                       "screen blocked by background {} times (<= {:.0f} ms)",
                       tWait, tStats.mBackgroundCount, tStats.mBackgroundMs, tStats.mBlockedCount, tStats.mBlockedMs));
}

void TerrainStore::workerLoop()
{
    // ARKA PLAN THREAD'i: XPLM yok. Log satirlari biriktirilir, ana thread yazar.
    {
        std::lock_guard<std::mutex> tLock(mMutex);
        mStatus = "Indexing scenery...";
    }
    std::error_code tError;
    std::filesystem::create_directories(mCacheDirectory, tError);
    const Clock::time_point tStart = Clock::now();
    mIndex.build(mXPlaneDirectory);
    mIndexReady.store(true);
    addLog(std::format("terrain index: {} DSF files in {} packs, {} tiles ({:.1f} s)", mIndex.fileCount(),
                       mIndex.packCount(), mIndex.tileCount(),
                       std::chrono::duration<double>(Clock::now() - tStart).count()));
    addLog(mIndex.worldMapReport());
    mCacheGrew = true;  // acilista bir kez: onceki oturumlardan sinir asilmis olabilir

    while (true)
    {
        TerrainImageKey tKey;
        Clock::time_point tPickedFirst;
        bool tIdle = false;
        {
            std::lock_guard<std::mutex> tLock(mMutex);
            mHasInFlight = false;
            tIdle = mRequests.empty() && mVectorRequests.empty();
        }
        if (serveVectorRequest())
        {
            continue;  // vektor istekleri kucuk ve yol katmani bekliyor: once onlar
        }
        if (serveBackgroundTile())
        {
            continue;  // ekranin istegi yok: bolge onbellegi / onceden yukleme
        }
        if (tIdle)
        {
            logImageTimes();
            if (mCacheGrew || mTrimWanted.exchange(false))
            {
                trimCache();
            }
        }
        {
            std::unique_lock<std::mutex> tLock(mMutex);
            mStatus.clear();
            mWake.wait(tLock,
                       [this]()
                       {
                           const bool tBackground = mRequests.empty() && (!mPrecache.empty() || !mPrefetch.empty());
                           return mStopping || tBackground || !mVectorRequests.empty() || mTrimWanted.load() ||
                                  (!mRequests.empty() && mResults.size() < kMaxQueuedResults);
                       });
            if (mStopping)
            {
                return;
            }
            if (!pickRequest(&tKey))
            {
                continue;  // sadece vektor istegi vardi (dongunun basinda karsilanir)
            }
            const auto tState = mStates.find(SceneryIndex::tileKey(tKey.mWest, tKey.mSouth));
            if (tState != mStates.end() && tState->second != TerrainTileState::Ready &&
                tState->second != TerrainTileState::Unknown)
            {
                continue;  // deniz / veri yok / bozuk: goruntu uretilmez
            }
            mHasInFlight = true;
            mInFlight = tKey;
            tPickedFirst = mPickedFirst;
        }
        const double tQueueMs = millisecondsSince(tPickedFirst);

        const TerrainRaster* tRaster = rasterFor(tKey.mWest, tKey.mSouth);
        if (!tRaster)
        {
            continue;
        }
        TerrainImage tImage;
        tImage.mKey = tKey;
        tImage.mSize = imageSize(tKey.mLevel);
        const Clock::time_point tImageStart = Clock::now();
        buildImageFor(tKey, *tRaster, &tImage);
        if (tImage.mRgba.empty())
        {
            continue;  // yakin zoom ucgenleri okunamadi
        }
        noteImageTime(tKey.mLevel, millisecondsSince(tImageStart));
        {
            TerrainProfile::ScopedStage tStage(TerrainProfile::Stage::Bleed);
            TerrainRaster::bleedTransparent(tImage.mSize, &tImage.mRgba);  // mipmap kenarlari koyulasmasin
        }
        // Gecikme olcumu: ilk istekten sonuca kadar
        if (tKey.mLevel >= 0 && tKey.mLevel < kLevelCount)
        {
            const double tReadyMs = millisecondsSince(tPickedFirst);
            ++mStats.mLatencyCount[tKey.mLevel];
            mStats.mQueueMs[tKey.mLevel] += tQueueMs;
            mStats.mReadyMs[tKey.mLevel] += tReadyMs;
            mStats.mReadyMaxMs[tKey.mLevel] = std::max(mStats.mReadyMaxMs[tKey.mLevel], tReadyMs);
        }

        std::lock_guard<std::mutex> tLock(mMutex);
        mResults.push_back(std::move(tImage));
    }
}
