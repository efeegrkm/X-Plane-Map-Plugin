// Zemin (DSF arazisi) icin arka plan iscisi. X-Plane'e bagimli DEGIL. Disk onbellegindeki
// her karo, uretildigi DSF'lerin (yol + boyut + degisiklik zamani) listesiyle etiketlenir;
// sahne degisirse etiket tutmaz ve karo yeniden uretilir.
#ifndef PRIME_MAP_SRC_DATA_TERRAINSTORE_H
#define PRIME_MAP_SRC_DATA_TERRAINSTORE_H

#include <chrono>
#include <map>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "data/SceneryIndex.h"
#include "data/TerrainClasses.h"

class ElevationGrid;
class TerrainRaster;
class TerrainVectors;
class TerrainTriangles;
class DsfReader;

struct TerrainImageKey
{
    int mWest = 0;
    int mSouth = 0;
    int mLevel = 0;
    int mChunk = 0;  // seviye 3: 0..15 (satir * 4 + sutun, satir 0 = kuzey); digerlerinde 0
    int mStyle = 0;  // Start'a verilen bicim listesindeki sira
    int mAreas = 0;  // TerrainRaster::kArea... bitleri: vektor alanlari islensin mi
    int mPalette = 0;  // ayar kumesi numarasi (addPalette)

    bool operator==(const TerrainImageKey& pOther) const
    {
        return mWest == pOther.mWest && mSouth == pOther.mSouth && mLevel == pOther.mLevel &&
               mChunk == pOther.mChunk && mStyle == pOther.mStyle && mAreas == pOther.mAreas &&
               mPalette == pOther.mPalette;
    }
};

struct TerrainImageKeyHash
{
    size_t operator()(const TerrainImageKey& pKey) const
    {
        // carpmasiz birlesim: karo * 8 seviye * 32768 parca * 8 bicim * 4 alan
        size_t tHash = static_cast<size_t>(SceneryIndex::tileKey(pKey.mWest, pKey.mSouth));
        tHash = tHash * 8 + static_cast<size_t>(pKey.mLevel);
        tHash = tHash * 32768 + static_cast<size_t>(pKey.mChunk);
        tHash = tHash * 8 + static_cast<size_t>(pKey.mStyle);
        tHash = tHash * 4 + static_cast<size_t>(pKey.mAreas & 3);
        return tHash * 31 + static_cast<size_t>(pKey.mPalette);
    }
};

struct TerrainImage
{
    TerrainImageKey mKey;
    int mSize = 0;
    std::vector<uint8_t> mRgba;  // satir 0 = kuzey
    // Uretimde rakimi olmayan komsu karolar; bos degilse karo kenar golgesi tek tarafli
    // hesaplandi, komsu hazir olunca yenilenmeli.
    std::vector<int> mMissingNeighbors;
};

enum class TerrainTileState
{
    Unknown,
    Ready,
    Ocean,   // temel mesh yok, bolge kurulu: deniz
    NoData,  // bolge kurulu degil
    Failed,  // DSF okunamadi
};

class TerrainStore
{
public:
    static const int kLevelCount = 6;
    static const int kDetailLevel = 4;  // DSF ucgenlerinden boyanan yakin seviye (1/32 derece)
    static const int kFineLevel = 5;    // ayni ucgenlerden kucuk parcalar (1/128 derece)
    static int imageSize(int pLevel);
    static int chunksPerSide(int pLevel);

    TerrainStore();
    ~TerrainStore();

    TerrainStore(const TerrainStore&) = delete;
    TerrainStore& operator=(const TerrainStore&) = delete;

    // pStyles: goruntu bicimleri (mStyle bu listenin sirasi; bossa hep Classes).
    void start(const std::string& pXPlaneDirectory, const std::string& pCacheDirectory, const TerrainPalette& pPalette,
               const std::vector<TerrainImageStyle>& pStyles = {});
    void stop();

    // Disk onbellegi siniri (bayt, 0 = sinirsiz). Isci bos kalinca LRU karolari siler;
    // sinir calisirken degistirilebilir.
    void setCacheLimit(uint64_t pBytes);

    // Bellek ici listelerinin boyu bu oranla carpilir (1.0 = tek harita); acik harita
    // sayisina gore buyutulur ki haritalar birbirinin karolarini itmesin.
    void setMemoryScale(double pScale);

    // Yeni zemin ayar kumesi tanitir, numarasini doner; Start'in paleti 0'dir.
    // Ayni paleti iki kez eklememek cagiranin isidir.
    int addPalette(const TerrainPalette& pPalette);

    bool isIndexReady() const;
    TileCoverage coverageOf(int pWest, int pSouth) const;  // dizin hazir degilse NoData
    TerrainTileState stateOf(int pWest, int pSouth) const;

    void beginFrame();                          // her karede Request'lerden once
    void Request(const TerrainImageKey& pKey);  // ayni karede tekrar istemek zararsiz
    bool popImage(TerrainImage* pOut);
    bool isIdle() const;
    std::string statusText() const;
    std::string takeLogLines();
    bool hasElevation(int pWest, int pSouth) const;  // hazir olan hazir kalir (eksik komsu yenilemesi icin)

    // vektorler(yol katmani): hazir olunca popVectors verir; vektoru olmayan karodan (deniz,
    // veri yok, sadece overlay) hic sonuc gelmez.
    void requestVectors(int pWest, int pSouth);
    bool popVectors(std::shared_ptr<const TerrainVectors>* pOut);
    void forgetVectors(int pWest, int pSouth);  // katman bellekten attiginda tekrar istenebilsin

    // Ayni karede istenen goruntulerden merkeze yakin olan once uretilir
    void setFocus(double pLat, double pLon);
    // Arka plan: ekran istegi yokken bu karolarin disk onbellegini hazirlar (doku uretmez,
    // bellekteki karolari itmez).
    void setPrefetchTiles(const std::vector<int>& pTiles);
    void startPrecache(const std::vector<int>& pTiles);
    void cancelPrecache();
    bool precacheProgress(int* pDone, int* pTotal) const;

    // Teshis: bosta kalinca asama sureleri ve istek beklemeleri Log'a yazilir; sayaclar hep tutulur.
    void setDiagnostics(bool pOn);

    // Test/onizleme: raster'i (ve rakimini) bekleterek hazirlar; onbellek guncelse DSF'e dokunmaz.
    static bool buildRaster(const SceneryIndex& pIndex, int pWest, int pSouth, const std::string& pCacheDirectory,
                            TerrainRaster* pOut, std::string* pLog, TerrainTileState* pState,
                            ElevationGrid* pElevation = nullptr, TerrainVectors* pVectors = nullptr);

    // Asagidaki okuyucular DSF acmaz, yalnizca onbellekten okur; dosya yoksa/eskiyse false:
    static bool loadVectors(const SceneryIndex& pIndex, int pWest, int pSouth, const std::string& pCacheDirectory,
                            TerrainVectors* pOut);
    static bool loadElevation(const SceneryIndex& pIndex, int pWest, int pSouth, const std::string& pCacheDirectory,
                              ElevationGrid* pOut);

    // Yakin zoom icin temel arazi ucgenlerini DSF'ten okur (diske yazilmaz)
    static bool loadTriangles(const SceneryIndex& pIndex, int pWest, int pSouth, TerrainTriangles* pOut,
                              std::string* pLog);

private:
    struct Pending
    {
        // Istegin istendigi andaki odak uzakligi; coklu harita kendi merkezine gore siralar.
        double mDistance = 0.0;
        uint64_t mFrame;
        std::chrono::steady_clock::time_point mFirst;  // bekleme suresi olcumu
    };

    // Olcum sayaclari (sadece isci; LogImageTimes'ta yazilip sifirlanir)
    struct WorkStats
    {
        std::map<int, int> mRasterMisses;
        std::map<int, int> mTriangleLoads;
        std::map<int, int> mVectorMisses;
        int mElevationLoads = 0;
        double mRasterMissMs = 0.0;
        double mTriangleMs = 0.0;
        int mLatencyCount[6] = {};
        double mQueueMs[6] = {};    // ilk istekten iscinin almasina kadar
        double mReadyMs[6] = {};    // ilk istekten sonucun hazir olmasina kadar
        double mReadyMaxMs[6] = {};
        int mBlockedCount = 0;      // arka plan karosu bitene kadar bekleyen ekran istekleri
        double mBlockedMs = 0.0;
        double mBackgroundMs = 0.0;
        int mBackgroundCount = 0;
    };

    void workerLoop();
    bool pickRequest(TerrainImageKey* pOut);
    TerrainRaster* rasterFor(int pWest, int pSouth);
    // Bellekten/onbellekten rakim; pLevel kaba seviyeyse kucultulmus (coarse) izgara yeter.
    const ElevationGrid* elevationFor(int pWest, int pSouth, int pLevel);
    void keepElevation(int pWest, int pSouth, std::unique_ptr<ElevationGrid> pGrid);
    void trimCache();
    bool serveBackgroundTile();
    void keepCoarseElevation(int pKey, std::unique_ptr<ElevationGrid> pGrid);
    void buildImageFor(const TerrainImageKey& pKey, const TerrainRaster& pRaster, TerrainImage* pOut);
    std::shared_ptr<TerrainVectors> vectorsFor(int pWest, int pSouth);
    bool serveVectorRequest();
    void noteImageTime(int pLevel, double pMilliseconds);
    void logImageTimes();
    TerrainTriangles* trianglesFor(int pWest, int pSouth);

    // Adaylar icinde overlay olmayan ilk DSF'i acar (pData okuyucu boyunca yasamali)
    static bool openBaseMesh(const std::vector<DsfCandidate>& pCandidates, const std::string& pName,
                             std::vector<uint8_t>* pData, DsfReader* pReader, std::string* pPath,
                             std::string* pLog);
    void setState(int pWest, int pSouth, TerrainTileState pState);
    void addLog(const std::string& pLine);

    std::string mXPlaneDirectory;
    std::string mCacheDirectory;
    std::atomic<uint64_t> mCacheLimit{0};
    std::atomic<bool> mTrimWanted{false};
    bool mCacheGrew = false;  // sadece isci: son kirpmadan beri yeni karo uretildi
    std::vector<TerrainPalette> mPalettes;  // mMutex ile; 0 = Start'a verilen
    std::atomic<double> mMemoryScale{1.0};
    size_t scaledLimit(size_t pBase) const;
    std::vector<TerrainImageStyle> mStyles;
    SceneryIndex mIndex;
    std::atomic<bool> mIndexReady{false};

    std::thread mThread;
    mutable std::mutex mMutex;
    std::condition_variable mWake;
    bool mStopping = false;
    uint64_t mFrame = 0;
    std::unordered_map<TerrainImageKey, Pending, TerrainImageKeyHash> mRequests;
    std::deque<TerrainImage> mResults;
    std::unordered_map<int, TerrainTileState> mStates;
    std::string mLog;
    std::string mStatus;

    // Odak ve arka plan kuyruklari (mMutex ile)
    double mFocusLat = 0.0;
    double mFocusLon = 0.0;
    std::deque<int> mPrefetch;
    std::deque<int> mPrecache;
    int mPrecacheDone = 0;
    int mPrecacheTotal = 0;
    std::unordered_set<int> mBackgroundDone;

    bool mHasInFlight = false;
    TerrainImageKey mInFlight;
    std::unordered_set<int> mElevationReady;
    std::unordered_map<int, uint64_t> mVectorRequests;  // karo -> son istendigi kare
    std::unordered_set<int> mVectorsServed;
    std::deque<std::shared_ptr<const TerrainVectors>> mVectorResults;

    // Sadece isci: bellek ici raster onbellegi (en son kullanilan basta)
    std::list<std::pair<int, std::unique_ptr<TerrainRaster>>> mRasters;

    // Rakim iki katman: tam izgara (karo ~5 MB, kMaxElevationTiles) ve kaba izgara (~0,5 MB,
    // kMaxCoarseElevationTiles). Tam listeden atilan izgara kucultulup kaba listeye gecer.
    std::list<std::pair<int, std::unique_ptr<ElevationGrid>>> mElevations;
    std::list<std::pair<int, std::unique_ptr<ElevationGrid>>> mCoarseElevations;

    // vektorler(sadece isci; yol katmanina paylasilir, uretildikten sonra degismez)
    std::list<std::pair<int, std::shared_ptr<TerrainVectors>>> mVectors;

    WorkStats mStats;
    std::atomic<bool> mDiagnostics{false};
    std::chrono::steady_clock::time_point mPickedFirst;
    int mImageCounts[kLevelCount] = {};
    double mImageMilliseconds[kLevelCount] = {};

    // Yakin zoom ucgenleri (karo basina ~30 MB; en son kullanilan basta)
    std::list<std::pair<int, std::unique_ptr<TerrainTriangles>>> mTriangles;
};
#endif  // PRIME_MAP_SRC_DATA_TERRAINSTORE_H
