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
// Bir karoda temel mesh yoksa (hic DSF yok ya da sadece overlay var):
//   X-Plane'in dunya tablosu (Resources/dsf_files, WorldTileMap) kullanilir:
//         karo orada yoksa DENIZ, varsa VERI YOK (kara ama sahnesi kurulu degil).
//         Tablo, Global Scenery'de kurulu karolarla dogrulanir (hepsi "var"
//         olmali); dosya yoksa, boyutu farkliysa veya dogrulama tutmazsa:
//   yedek: karonun 10x10 derece klasoru bir Global Scenery paketinde varsa
//         DENIZ (Laminar tamamen deniz olan karolari dagitmaz), yoksa VERI YOK.
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "data/WorldTileMap.h"

struct DsfCandidate
{
    std::string mPath;
    uint64_t mSize = 0;
    int64_t mModified = 0;  // dosyanin degisiklik zamani (onbellek anahtari icin)
};

enum class TileCoverage
{
    hasDsf,  // en az bir aday DSF var
    Ocean,   // bolge kurulu ama DSF yok: deniz
    NoData,  // bolge kurulu degil
};

class SceneryIndex
{
public:
    // <X-Plane>/Custom scenery(scenery_packs.ini sirasi) + <X-Plane>/Global Scenery/*
    void build(const std::string& pXPlaneDirectory);

    // Paket klasorleriyle kur (testler ve Build icin). pCustomPacks once,
    // pGlobalPacks sonra taranir; deniz tespiti sadece pGlobalPacks'e bakar.
    void buildFromPacks(const std::vector<std::string>& pCustomPacks, const std::vector<std::string>& pGlobalPacks);

    const std::vector<DsfCandidate>* candidatesFor(int pWest, int pSouth) const;
    TileCoverage coverageOf(int pWest, int pSouth) const;
    // Temel mesh'i olmayan karo (DSF yok ya da sadece overlay): Ocean veya noData(dosya basindaki kural)
    TileCoverage noBaseCoverage(int pWest, int pSouth) const;
    // Karonun 10x10 derece klasoru bir Global Scenery paketinde var mi (bolge kurulu mu)
    bool isRegionInstalled(int pWest, int pSouth) const;

    // Dunya tablosunu verir ve Global Scenery karolariyla dogrular (buildFromPacks'ten SONRA).
    // Dogrulama tutmazsa tablo kullanilmaz (yedek kural). Sonuc: worldMapReport().
    void setWorldMap(const WorldTileMap& pXMap);
    bool usesWorldMap() const;
    const std::string& worldMapReport() const;

    int tileCount() const;    // en az bir DSF'i olan karo sayisi
    int fileCount() const;    // toplam DSF dosyasi
    int packCount() const;    // DSF iceren paket sayisi

    static int tileKey(int pWest, int pSouth);
    static std::string tileName(int pWest, int pSouth);    // "+47-123"
    static std::string folderName(int pWest, int pSouth);  // "+40-130"
    // "+47-123.dsf" -> (-123, 47); bicim yanlissa false
    static bool parseTileName(const std::string& pFileName, int* pOutWest, int* pOutSouth);

private:
    void scanPack(const std::string& pPackDirectory, bool pGlobal);

    std::unordered_map<int, std::vector<DsfCandidate>> mTiles;
    std::unordered_set<int> mInstalledFolders;  // Global Scenery'de var olan 10x10 klasorler
    std::unordered_set<int> mGlobalTiles;       // Global Scenery'de DSF'i olan karolar (tablo dogrulamasi)
    WorldTileMap mWorldMap;                     // bossa kullanilmiyor
    std::string mWorldMapReport;
    int mFileCount = 0;
    int mPackCount = 0;
};
