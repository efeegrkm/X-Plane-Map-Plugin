// Zemin disk onbelleginin boyut siniri. X-Plane'e bagimli DEGIL.
//
// cache/terrain/ altinda karo basina birkac dosya var (ayni ad, farkli uzanti):
//   +47-123.terrain  +47-123.elevation  +47-123.vectors  +47-123.nobase
// Bir karonun dosyalari BIRLIKTE silinir (yarim karo kalmasin). Siralama "en son
// kullanilan": okunan dosyanin degistirilme zamani simdiye cekilir
// (touchCacheFile); Windows'ta "son erisim zamani" guvenilir degil.
//
// Sinirin ustundeyse en uzun suredir kullanilmayan karolar, toplam sinirin
// %90'ina inene kadar silinir. Karo adi olmayan dosyalara dokunulmaz.
// Sadece zemin iscisinin thread'inden cagrilir: silinen dosya baska yerde okunmuyor.
#pragma once

#include <cstdint>
#include <string>

struct CacheTrimResult
{
    uint64_t mTotalBytes = 0;    // silmeden onceki toplam (karo dosyalari)
    uint64_t mRemovedBytes = 0;
    int mRemovedTiles = 0;
};

// Dosyanin degistirilme zamanini simdiye ceker (hata sessizce yok sayilir)
void touchCacheFile(const std::string& pPath);

// pDirectory sonunda '/' olabilir. pMaxBytes == 0: sinir yok (sadece toplam olculur).
CacheTrimResult trimTileCache(const std::string& pDirectory, uint64_t pMaxBytes);
