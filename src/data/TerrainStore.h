// TerrainStore.h
// Zemin (DSF arazisi) icin arka plan iscisi. X-Plane'e bagimli DEGIL.
//
// Akis:
//   1. Basta bir kez: SceneryIndex kurulur (hangi karoda hangi DSF'ler var).
//   2. Katman her karede ekranda gerekli goruntuleri ister (Request).
//   3. Isci en son istenenden baslayarak: karonun sinif raster'ini bulur
//      (bellekte -> disk onbellegi -> DSF'ten uretim) ve istenen seviyede bir
//      RGBA goruntu hazirlar.
//   4. Ana thread hazir goruntuleri alir (PopImage) ve dokuya yukler.
//
// Senkron kalma: disk onbellegindeki her karo, uretildigi DSF'lerin
// (yol + boyut + degisiklik zamani) listesiyle etiketlidir. Sahne eklenir,
// cikarilir veya degisirse etiket tutmaz ve karo yeniden uretilir.
//
// Seviyeler (goruntu boyutu):  0 = 64, 1 = 256, 2 = 1024 (tum karo),
//                              3 = 4096'nin 4x4 parcasindan biri (her biri 1024)
//                              4 = yakin zoom: 32x32 parcadan biri (1/32 derece,
//                                  1024 px), raster'dan degil DSF ucgenlerinden
//                                  boyanir (TerrainTriangles) -> keskin kenarlar
//                              5 = cok yakin zoom: 128x128 parcadan biri (1/128
//                                  derece, 1024 px; ~0,85 m/piksel), ayni ucgenlerden
// Her goruntunun saydam pikselleri komsu rengiyle doldurulur (BleedTransparent):
// mipmap ve dogrusal suzme kenarlarda koyu sacak yapmasin.
#pragma once

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
#include <vector>

#include "data/SceneryIndex.h"
#include "data/TerrainClasses.h"

class TerrainRaster;
class TerrainTriangles;
class DsfReader;

struct TerrainImageKey
{
    int mWest = 0;
    int mSouth = 0;
    int mLevel = 0;
    int mChunk = 0;  // seviye 3: 0..15 (satir * 4 + sutun, satir 0 = kuzey); digerlerinde 0
    int mStyle = 0;  // Start'a verilen bicim listesindeki sira (harita modu)

    bool operator==(const TerrainImageKey& pOther) const
    {
        return mWest == pOther.mWest && mSouth == pOther.mSouth && mLevel == pOther.mLevel &&
               mChunk == pOther.mChunk && mStyle == pOther.mStyle;
    }
};

struct TerrainImageKeyHash
{
    size_t operator()(const TerrainImageKey& pKey) const
    {
        // karo * (8 seviye) * (32768 parca; seviye 5 = 16384) * (8 bicim): carpisma yok
        size_t tHash = static_cast<size_t>(SceneryIndex::TileKey(pKey.mWest, pKey.mSouth));
        tHash = tHash * 8 + static_cast<size_t>(pKey.mLevel);
        tHash = tHash * 32768 + static_cast<size_t>(pKey.mChunk);
        return tHash * 8 + static_cast<size_t>(pKey.mStyle);
    }
};

struct TerrainImage
{
    TerrainImageKey mKey;
    int mSize = 0;
    std::vector<uint8_t> mRgba;  // satir 0 = kuzey
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
    static int ImageSize(int pLevel);
    static int ChunksPerSide(int pLevel);

    TerrainStore();
    ~TerrainStore();

    TerrainStore(const TerrainStore&) = delete;
    TerrainStore& operator=(const TerrainStore&) = delete;

    // Isciyi baslatir. pCacheDirectory yoksa olusturulur. pStyles: goruntu
    // bicimleri (TerrainImageKey::mStyle bu listenin sirasi; bossa hep Classes).
    void Start(const std::string& pXPlaneDirectory, const std::string& pCacheDirectory, const TerrainPalette& pPalette,
               const std::vector<TerrainImageStyle>& pStyles = {});
    void Stop();

    bool IsIndexReady() const;
    TileCoverage CoverageOf(int pWest, int pSouth) const;  // dizin hazir degilse NoData
    TerrainTileState StateOf(int pWest, int pSouth) const;

    // --- Ana thread ---
    void BeginFrame();                          // her karede Request'lerden once
    void Request(const TerrainImageKey& pKey);  // ayni karede tekrar istemek zararsiz
    bool PopImage(TerrainImage* pOut);
    bool IsIdle() const;                        // bekleyen istek ve sonuc yok
    std::string StatusText() const;             // alt seritte gosterilecek kisa durum
    std::string TakeLogLines();                 // iscinin biriktirdigi log satirlari

    // Test ve onizleme icin: karonun raster'ini bekleterek hazirlar (iscisiz)
    static bool BuildRaster(const SceneryIndex& pIndex, int pWest, int pSouth, const std::string& pCacheDirectory,
                            TerrainRaster* pOut, std::string* pLog, TerrainTileState* pState);

    // Yakin zoom icin karonun temel arazi ucgenlerini DSF'ten okur (diske yazilmaz)
    static bool LoadTriangles(const SceneryIndex& pIndex, int pWest, int pSouth, TerrainTriangles* pOut,
                              std::string* pLog);

private:
    struct Pending
    {
        uint64_t mFrame;
    };

    void WorkerLoop();
    bool PickRequest(TerrainImageKey* pOut);
    TerrainRaster* RasterFor(int pWest, int pSouth);
    TerrainTriangles* TrianglesFor(int pWest, int pSouth);

    // Adaylar icinde overlay olmayan ilk DSF'i acar (pData okuyucu boyunca yasamali)
    static bool OpenBaseMesh(const std::vector<DsfCandidate>& pCandidates, const std::string& pName,
                             std::vector<uint8_t>* pData, DsfReader* pReader, std::string* pPath,
                             std::string* pLog);
    void SetState(int pWest, int pSouth, TerrainTileState pState);
    void AddLog(const std::string& pLine);

    std::string mXPlaneDirectory;
    std::string mCacheDirectory;
    TerrainPalette mPalette;
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

    // Sadece iscinin kullandigi bellek ici raster onbellegi (en son kullanilan basta)
    std::list<std::pair<int, std::unique_ptr<TerrainRaster>>> mRasters;

    // Yakin zoom ucgenleri (karo basina ~30 MB; en son kullanilan basta)
    std::list<std::pair<int, std::unique_ptr<TerrainTriangles>>> mTriangles;
};
