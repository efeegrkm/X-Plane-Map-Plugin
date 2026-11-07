// CacheTrim.h
// Zemin disk onbelleginin boyut siniri (0.18). X-Plane'e bagimli DEGIL.
//
// cache/terrain/ altinda karo basina birkac dosya var (ayni ad, farkli uzanti):
//   +47-123.terrain  +47-123.elevation  +47-123.vectors  +47-123.nobase
// Bir karonun dosyalari BIRLIKTE silinir (yarim karo kalmasin). Siralama "en son
// kullanilan": onbellekten okunan dosyanin degistirilme zamani okununca simdiye
// cekilir (TouchCacheFile). Windows'ta "son erisim zamani" guvenilir degil
// (cogu zaman kapali), o yuzden degistirilme zamani kullanilir.
//
// Toplam sinirin ustundeyse en uzun suredir kullanilmayan karolar silinir, ta ki
// toplam sinirin %90'ina insin (her yeni karoda tekrar silmeye baslamasin).
// Karo adi olmayan dosyalara (xplane_palette.txt ...) dokunulmaz.
// Sadece zemin iscisinin thread'inden cagrilir: silinen dosya o an baska yerde okunmuyor.
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
void TouchCacheFile(const std::string& pPath);

// pDirectory sonunda '/' olabilir. pMaxBytes == 0: sinir yok (sadece toplam olculur).
CacheTrimResult TrimTileCache(const std::string& pDirectory, uint64_t pMaxBytes);
