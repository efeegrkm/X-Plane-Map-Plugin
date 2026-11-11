# X-Plane Map Plugin — 0.22 (ekip kütüphane kalıbı)

Bu dal, kütüphanenin **ekip kütüphane kalıbına** (SHARED DLL) geçilmiş asıl
sürümüdür. Ağaç üç bağımsız projeden oluşur:

```
prime_map_api/     harita kütüphanesi + genel API (prime_map SHARED → prime_map.dll)
prime_map_plugin/  resmî X-Plane eklentisi (ince kabuk)
plugin-api-test/   API'yi ayrı bir eklentide deneyen test eklentisi
```

Tüketim (0.22): SDK paketi yoktur; kütüphane aynı CMake oturumuna
`add_subdirectory(<prime_map_api yolu> prime_map_build)` ile katılır ve hedef
adına linklenir (`target_link_libraries(<hedef> PRIVATE prime_map)`). Include:
`#include <prime_map_api/PrimeMap.h>`. Giriş noktası
`PrimeMap::MapFactory::CreateMap` (giriye uyumluluk: `Map::Open`).
Derlemeden sonra `prime_map.dll`, tüketici eklentinin (`win.xpl`) yanına kopyalanır.

Yöntem: SDK paketi/install yoktur (0.21 düzeni için `feature/api` dalına bakınız).
CRT `/MD` sabittir. Sürüm bilgisi CMake `project(VERSION)`'tan her iki DLL'e
(Windows sürüm kaynağı, .rc) gömülür.
