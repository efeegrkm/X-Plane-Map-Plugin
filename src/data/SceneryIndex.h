// SceneryIndex.h
// Bilgisayarda kurulu DSF dosyalarinin dizini: her 1x1 derece karo icin aday
// DSF'ler, X-Plane'in kullandigi oncelik sirasiyla. X-Plane'e bagimli DEGIL.
//
// X-Plane'in kurali (bir karonun zemini): paketler oncelik sirasiyla taranir,
// "overlay" OLMAYAN ilk DSF karonun temel mesh'idir; digerleri ya overlay'dir
// (ustune serilir) ya da gormezden gelinir. Oncelik sirasi:
//   1. Custom Scenery: scenery_packs.ini'deki etkin paketler, yukaridan asagi
//   2. Global Scenery: alt klasorleri (X-Plane 11 Global Scenery, Demo Areas ...)
// Overlay olup olmadigi dosyanin icinde yazar (sim/overlay); dizin bunu bilmez,
// TerrainStore dosyayi acinca karar verir.
//
// Bir karoda hic DSF yoksa: o karonun 10x10 derece klasoru bir Global Scenery
// paketinde varsa karo DENIZDIR (Laminar tamamen deniz olan karolari dagitmaz);
// klasor de yoksa o bolge bu bilgisayarda kurulu degildir (veri yok).
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct DsfCandidate
{
    std::string mPath;
    uint64_t mSize = 0;
    int64_t mModified = 0;  // dosyanin degisiklik zamani (onbellek anahtari icin)
};

enum class TileCoverage
{
    HasDsf,  // en az bir aday DSF var
    Ocean,   // bolge kurulu ama DSF yok: deniz
    NoData,  // bolge kurulu degil
};

class SceneryIndex
{
public:
    // <X-Plane>/Custom Scenery (scenery_packs.ini sirasi) + <X-Plane>/Global Scenery/*
    void Build(const std::string& pXPlaneDirectory);

    // Paket klasorleriyle kur (testler ve Build icin). pCustomPacks once,
    // pGlobalPacks sonra taranir; deniz tespiti sadece pGlobalPacks'e bakar.
    void BuildFromPacks(const std::vector<std::string>& pCustomPacks, const std::vector<std::string>& pGlobalPacks);

    const std::vector<DsfCandidate>* CandidatesFor(int pWest, int pSouth) const;
    TileCoverage CoverageOf(int pWest, int pSouth) const;
    // Karonun 10x10 derece klasoru bir Global Scenery paketinde var mi (bolge kurulu mu)
    bool IsRegionInstalled(int pWest, int pSouth) const;

    int TileCount() const;    // en az bir DSF'i olan karo sayisi
    int FileCount() const;    // toplam DSF dosyasi
    int PackCount() const;    // DSF iceren paket sayisi

    static int TileKey(int pWest, int pSouth);
    static std::string TileName(int pWest, int pSouth);    // "+47-123"
    static std::string FolderName(int pWest, int pSouth);  // "+40-130"
    // "+47-123.dsf" -> (-123, 47); bicim yanlissa false
    static bool ParseTileName(const std::string& pFileName, int* pOutWest, int* pOutSouth);

private:
    void ScanPack(const std::string& pPackDirectory, bool pGlobal);

    std::unordered_map<int, std::vector<DsfCandidate>> mTiles;
    std::unordered_set<int> mInstalledFolders;  // Global Scenery'de var olan 10x10 klasorler
    int mFileCount = 0;
    int mPackCount = 0;
};
