// Zemin (DSF arazisi) icin arka plan iscisi. X-Plane'e bagimli DEGIL.
// Disk onbellegindeki her karo, uretildigi DSF'lerin (yol + boyut + degisiklik zamani)
// listesiyle etiketlenir; sahne degisirse etiket tutmaz ve karo yeniden uretilir.
#pragma once

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
    int mStyle = 0;  // Start'a verilen bicim listesindeki sira (harita modu)
    int mAreas = 0;  // TerrainRaster::kArea... bitleri: vektor alanlari (orman, yerlesim) islensin mi
    // Zemin ayarlari (renk, kabartma, su ...) harita basina olabilir. Her farkli
    // ayar kumesi depoya bir kez tanitilir (TerrainStore::addPalette) ve buradaki numarayla istenir.
    int mPalette = 0;

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
        // karo * (8 seviye) * (32768 parca; seviye 5 = 16384) * (8 bicim): carpisma yok
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
    // Goruntu uretilirken rakimi henuz olmayan komsu karolar (SceneryIndex::TileKey).
    // Bos degilse karo kenarinda golge tek tarafli farkla hesaplandi; komsu hazir olunca yenilenmeli.
    std::vector<int> mMissingNeighbors;
};

enum class TerrainTileState
{
    Unknown,   // henuz bakilmadi
    Ready,     // raster var (bellekte veya onbellekte)
    Ocean,     // temel mesh yok, bolge kurulu: deniz
    NoData,    // bolge bu bilgisayarda kurulu degil
    Failed,    // DSF okunamadi
};

class TerrainStore
{
public:
    static const int kLevelCount = 6;
    static const int kDetailLevel = 4;  // DSF ucgenlerinden boyanan yakin zoom seviyesi (1/32 derece)
    static const int kFineLevel = 5;    // ayni ucgenlerden daha kucuk parcalar (1/128 derece)
    static int imageSize(int pLevel);
    static int chunksPerSide(int pLevel);

    TerrainStore();
    ~TerrainStore();

    TerrainStore(const TerrainStore&) = delete;
    TerrainStore& operator=(const TerrainStore&) = delete;

    // Isciyi baslatir. pCacheDirectory yoksa olusturulur.
    // pStyles: goruntu bicimleri (TerrainImageKey::mStyle bu listenin sirasi; bossa hep Classes).
    void start(const std::string& pXPlaneDirectory, const std::string& pCacheDirectory, const TerrainPalette& pPalette,
               const std::vector<TerrainImageStyle>& pStyles = {});
    void stop();

    // Disk onbelleginin siniri (bayt, 0 = sinirsiz). Isci acilista ve yeni karo urettikten sonra
    // bosta kalinca en uzun suredir kullanilmayan karolari siler (CacheTrim). Calisirken de
    // degistirilebilir; isci bir sonraki bos aninda yeni sinira gore kirpar.
    void setCacheLimit(uint64_t pBytes);

    // Iscinin bellek ici listelerinin (raster, rakim, vektor, ucgen) boyu bu oranla carpilir
    // (1.0 = tek harita). Iki harita farkli yerlere bakarken birbirinin karolarini bellekten
    // itmesin diye acik harita sayisina gore buyutulur. Her an cagrilabilir.
    void setMemoryScale(double pScale);

    // Yeni bir zemin ayar kumesi (palet) tanitir, numarasini doner (TerrainImageKey::mPalette).
    // Start'a verilen palet 0 numaradir. Her an (ana thread) cagrilabilir; isci goruntuyu uretirken
    // kendi kopyasini alir. Ayni paleti iki kez eklememek cagiranin isidir.
    int addPalette(const TerrainPalette& pPalette);

    bool isIndexReady() const;
    TileCoverage coverageOf(int pWest, int pSouth) const;  // dizin hazir degilse NoData
    TerrainTileState stateOf(int pWest, int pSouth) const;

    void beginFrame();                          // her karede Request'lerden once
    void Request(const TerrainImageKey& pKey);  // ayni karede tekrar istemek zararsiz
    bool popImage(TerrainImage* pOut);
    bool isIdle() const;                        // bekleyen istek ve sonuc yok
    std::string statusText() const;             // alt seritte gosterilecek kisa durum
    std::string takeLogLines();                 // iscinin biriktirdigi log satirlari
    // Karonun rakimi iscide hazir mi (bir kez hazir olan hazir kalir; eksik komsu yenilemesi icin)
    bool hasElevation(int pWest, int pSouth) const;

    // vektorler(yol katmani): istek her karede tekrarlanabilir; hazir olunca popVectors verir.
    // Vektoru olmayan karo (deniz, veri yok, sadece overlay) icin hic sonuc gelmez.
    void requestVectors(int pWest, int pSouth);
    bool popVectors(std::shared_ptr<const TerrainVectors>* pOut);
    // Katman karonun vektorlerini bellekten attiginda: tekrar istenebilsin
    void forgetVectors(int pWest, int pSouth);

    // Ekran merkezi: ayni karede istenen goruntulerden (ayni seviyede) merkeze yakin olan once uretilir
    void setFocus(double pLat, double pLon);
    // Arka plan onbellegi: isci, ekranin istegi yokken bu karolarin disk onbellegini (raster, rakim,
    // vektor) hazirlar; doku uretmez, bellekteki karolari itmez. Once bolge listesi (startPrecache),
    // sonra onceden yukleme listesi (setPrefetchTiles). Karo = SceneryIndex::TileKey.
    void setPrefetchTiles(const std::vector<int>& pTiles);  // oncelik sirasiyla; oncekinin yerine gecer
    void startPrecache(const std::vector<int>& pTiles);
    void cancelPrecache();
    // Bolge onbellegi calisiyorsa true ve ilerleme
    bool precacheProgress(int* pDone, int* pTotal) const;

    // teshis(olcum): isci bosta kalinca asama sureleri, yeniden yuklemeler ve istek bekleme
    // sureleri log'a yazilir ("terrain profile ..."). Sayaclar her zaman tutulur (ucuz).
    void setDiagnostics(bool pOn);

    // Test ve onizleme icin: karonun raster'ini (ve pElevation verilmisse rakimini) bekleterek
    // hazirlar (iscisiz). Onbellekte ikisi de guncelse DSF'e dokunmaz.
    static bool buildRaster(const SceneryIndex& pIndex, int pWest, int pSouth, const std::string& pCacheDirectory,
                            TerrainRaster* pOut, std::string* pLog, TerrainTileState* pState,
                            ElevationGrid* pElevation = nullptr, TerrainVectors* pVectors = nullptr);

    // Karonun vektorlerini SADECE onbellekten okur (DSF acmaz). Dosya yoksa / eskiyse false.
    static bool loadVectors(const SceneryIndex& pIndex, int pWest, int pSouth, const std::string& pCacheDirectory,
                            TerrainVectors* pOut);

    // Karonun rakimini SADECE onbellekten okur (DSF acmaz). Dosya yoksa / eskiyse false.
    static bool loadElevation(const SceneryIndex& pIndex, int pWest, int pSouth, const std::string& pCacheDirectory,
                              ElevationGrid* pOut);

    // Yakin zoom icin karonun temel arazi ucgenlerini DSF'ten okur (diske yazilmaz)
    static bool loadTriangles(const SceneryIndex& pIndex, int pWest, int pSouth, TerrainTriangles* pOut,
                              std::string* pLog);

private:
    struct Pending
    {
        // Istegin, ISTENDIGI andaki odaga (isteyen haritanin merkezi) uzakligi. Birden cok harita
        // ayni depoyu kullanir: her harita kendi merkezine gore siralanir.
        double mDistance = 0.0;
        uint64_t mFrame;
        std::chrono::steady_clock::time_point mFirst;  // ilk istendigi an (bekleme suresi olcumu)
    };

    // Olcum sayaclari (sadece isci; LogImageTimes'ta yazilip sifirlanir)
    struct WorkStats
    {
        std::map<int, int> mRasterMisses;    // karo -> bellekte olmadigi icin yuklenme / uretilme sayisi
        std::map<int, int> mTriangleLoads;   // karo -> DSF ucgen yukleme sayisi
        std::map<int, int> mVectorMisses;    // karo -> vektor yukleme sayisi
        int mElevationLoads = 0;             // rakimin diskten okunmasi
        double mRasterMissMs = 0.0;
        double mTriangleMs = 0.0;
        int mLatencyCount[6] = {};           // seviye basina: uretilen goruntu
        double mQueueMs[6] = {};             // ilk istekten iscinin almasina kadar
        double mReadyMs[6] = {};             // ilk istekten sonucun hazir olmasina kadar
        double mReadyMaxMs[6] = {};
        int mBlockedCount = 0;               // arka plan karosu bitene kadar bekleyen ekran istegi olan turlar
        double mBlockedMs = 0.0;             // o turlarin toplam suresi (ekranin bekledigi ust sinir)
        double mBackgroundMs = 0.0;          // butun arka plan karolarinin suresi
        int mBackgroundCount = 0;
    };

    void workerLoop();
    bool pickRequest(TerrainImageKey* pOut);
    TerrainRaster* rasterFor(int pWest, int pSouth);
    // Bellekten ya da onbellekten rakim (DSF acmaz); yoksa nullptr. Bulunursa LRU'nun basina gecer.
    // pLevel: goruntunun okuyacagi piramit seviyesi; kCoarseElevationLevel ve ustu icin kaba
    // (ince seviyeleri birakilmis) izgara yeter.
    const ElevationGrid* elevationFor(int pWest, int pSouth, int pLevel);
    void keepElevation(int pWest, int pSouth, std::unique_ptr<ElevationGrid> pGrid);
    void trimCache();  // isci: disk onbellegini mCacheLimit'e indirir
    bool serveBackgroundTile();  // isci: ekran istegi yoksa bir arka plan karosu (true: is yapildi)
    void keepCoarseElevation(int pKey, std::unique_ptr<ElevationGrid> pGrid);
    void buildImageFor(const TerrainImageKey& pKey, const TerrainRaster& pRaster, TerrainImage* pOut);
    // Vektorler: bellekte yoksa karonun raster'iyla birlikte yuklenir / uretilir (RasterFor)
    std::shared_ptr<TerrainVectors> vectorsFor(int pWest, int pSouth);
    bool serveVectorRequest();  // bekleyen bir vektor istegini karsilar (varsa true)
    void noteImageTime(int pLevel, double pMilliseconds);  // seviye basina sure (ozet log)
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
    std::atomic<bool> mTrimWanted{false};  // sinir calisirken degisti: isci bos aninda kirpsin
    bool mCacheGrew = false;  // sadece isci: son kirpmadan beri yeni karo uretildi
    std::vector<TerrainPalette> mPalettes;  // mMutex ile; 0 = Start'a verilen
    std::atomic<double> mMemoryScale{1.0};
    size_t scaledLimit(size_t pBase) const;  // pBase x mMemoryScale (en az pBase)
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
    std::unordered_set<int> mBackgroundDone;  // bu oturumda disk onbellegi hazirlanmis karolar

    bool mHasInFlight = false;            // isci su an bu goruntuyu uretiyor (ayni istek tekrar alinmaz)
    TerrainImageKey mInFlight;
    std::unordered_set<int> mElevationReady;  // rakimi hazir olmus karolar (hasElevation)
    std::unordered_map<int, uint64_t> mVectorRequests;  // karo -> son istendigi kare
    std::unordered_set<int> mVectorsServed;             // sonucu verilmis karolar (tekrar uretilmez)
    std::deque<std::shared_ptr<const TerrainVectors>> mVectorResults;

    // Sadece iscinin kullandigi bellek ici raster onbellegi (en son kullanilan basta)
    std::list<std::pair<int, std::unique_ptr<TerrainRaster>>> mRasters;

    // Rakim, iki katman (en son kullanilan basta):
    //   tam izgara (karo basina ~5 MB; yakin zoom ve 3 x 3 komsuluk) en fazla kMaxElevationTiles;
    //   kaba izgara (sadece seviye 2+, ~0,5 MB; uzak zoom, ekranda cok karo) en fazla kMaxCoarseElevationTiles.
    // Tam listeden atilan izgara silinmez, kucultulup kaba listeye gecer.
    std::list<std::pair<int, std::unique_ptr<ElevationGrid>>> mElevations;
    std::list<std::pair<int, std::unique_ptr<ElevationGrid>>> mCoarseElevations;

    // vektorler(sadece isci; yol katmanina paylasilir, uretildikten sonra degismez), en son kullanilan basta
    std::list<std::pair<int, std::shared_ptr<TerrainVectors>>> mVectors;

    WorkStats mStats;
    std::atomic<bool> mDiagnostics{false};
    std::chrono::steady_clock::time_point mPickedFirst;  // pickRequest'in sectigi istegin ilk zamani
    // Goruntu sureleri (sadece isci): seviye basina adet ve toplam ms; bos kalinca loglanir
    int mImageCounts[kLevelCount] = {};
    double mImageMilliseconds[kLevelCount] = {};

    // Yakin zoom ucgenleri (karo basina ~30 MB; en son kullanilan basta)
    std::list<std::pair<int, std::unique_ptr<TerrainTriangles>>> mTriangles;
};
