// WorldTileMap.h
// X-Plane'in "Resources/dsf_files" dosyasi: dunyadaki her 1x1 derece karo icin
// "X-Plane'in tam global sahnesinde bu karonun DSF'i var mi". X-Plane'e
// bagimli DEGIL.
//
// GERCEK VERI (2026-11-06, XP11.55 demo; tam surumde de ayni dosya beklenir):
//   boyut  : 5 bayt baslik (anlami bilinmiyor) + 360 x 180 bayt = 64.805 bayt
//   sira   : BOYLAM oncelikli: indeks = (boylam + 180) * 180 + (enlem + 90)
//   deger  : 255 = karonun DSF'i var (kara / kiyi), 0 = yok (acik deniz)
// Dogrulama: kurulu 35 demo karosunun hepsi 255; Ankara, Istanbul 255;
// Karadeniz ortasi, Pasifik, Atlantik, Antarktika ici 0. Dosya bir karonun bu
// bilgisayarda KURULU olup olmadigini soylemez (onu SceneryIndex bilir).
//
// Kullanim (SceneryIndex): karoda kurulu DSF yoksa 0 -> deniz, 255 -> veri yok
// (kara ama sahnesi kurulu degil; haritada notr "veri yok" rengi).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

class WorldTileMap
{
public:
    static const size_t kHeaderSize = 5;
    static const size_t kFileSize = kHeaderSize + 360 * 180;

    // Dosyayi okur; boyut tutmazsa false (harita bos kalir, cagiran yedek kurala doner)
    bool Load(const std::string& pPath);
    // Bellekteki baytlardan (testler icin); boyut tutmazsa false
    bool LoadBytes(const std::vector<uint8_t>& pBytes);

    bool IsLoaded() const;
    void Clear();

    // Karonun global sahnede DSF'i var mi. pWest: -180..179, pSouth: -90..89. Yuklu degilse false.
    bool HasDsf(int pWest, int pSouth) const;
    int LandTileCount() const;  // 255 olan karo sayisi (log icin)

    static size_t IndexOf(int pWest, int pSouth);

private:
    std::vector<uint8_t> mTiles;  // 360 * 180, IndexOf sirasi; bossa yuklu degil
};
