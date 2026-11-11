# X-Plane Map Plugin — 0.21 (SDK'lı dağıtım)

Bu dal, ekip kütüphane kalıbına geçişten (0.22) önceki **SDK paketli dağıtım
düzeninin** son hâlidir. Ağaç üç bağımsız projeden oluşur:

```
prime_map_api/     harita kütüphanesi + genel API (prime_map STATIC; prime_map_MT[d].lib / prime_map_MD[d].lib)
prime_map_plugin/  resmî X-Plane eklentisi (SDK paketindeki statik kütüphaneye karşı linklenir)
plugin-api-test/   API'yi ayrı bir eklentide deneyen test eklentisi
```

Tüketim akışı (0.21):

1. `prime_map_api` derlenir: `cmake -S prime_map_api -B b_api -G Ninja -DCMAKE_BUILD_TYPE=Release`
2. SDK paketi üretilir: `cmake --install b_api --prefix <klasor>/prime_map_sdk`
   → `include/PrimeMap.h`, `lib/prime_map_MT.lib` (veya `/MD` ile `prime_map_MD.lib`), `docs/`, `examples/`
3. Tüketici eklenti `-DPRIME_MAP_SDK=<paket yolu>` ile derlenir ve
   `#include "PrimeMap.h"` ile API'yi kullanır; `prime_map_MT.lib` eklentinin içine gömülür.

Not: Bu anlık görüntü, 0.21 kaynak ağacı commit'lenmeden 0.22'ye geçildiği için
oturum kayıtlarından birebir kurtarılmıştır; tam derleme + 30.659 test ile doğrulanmıştır.
0.22'deki (ekip standardı, SHARED DLL) sürüm için `main` dalına bakınız.
