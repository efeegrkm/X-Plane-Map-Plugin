# 0.22 — Ekip kalıbına geçiş raporu (2026-11-10, gece otomatik çalışma)

> **Not (2026-11-11):** Bu rapor geçiş anının kaydıdır. Sonrasında `MapFactory`
> fabrika sınıfı, kütüphaneyi bir **nesne** yapan `prime::MapLibrary` ile değiştirildi
> (mentor geri bildirimi: Init bir nesneyi başlatmalı); namespace `prime` oldu,
> çoklu host ve çoklu harita desteği eklendi. Güncel API: `PrimeMap.h`.

Mentorun istediği yönde: `prime_map_api` "SDK paketi üreten statik kütüphane"den
**ekibin kalıbına** (SHARED DLL + `*global.h` export makrosu + fabrika sınıfı +
CMake hedef-adına tüketim) çevrildi. Analist subagent'in raporu ile birleştirildi.

## Yapılan değişiklikler

### prime_map_api
1. **`primemapglobal.h` (YENİ, repo kökü)** — ekibin `utilsglobal.h` deseni birebir:
   yerel `Q_DECL_EXPORT/IMPORT` tanımı + `PRIME_MAP_LIBRARY` → `PRIME_MAP_EXPORT`.
2. **`PrimeMap.h` repo köküne taşındı** (`src/api/` → kök; ekipte başlıklar repo kökünde).
   Tüketici include'u: `#include <prime_map_api/PrimeMap.h>` (ekibin
   `<prime-utils/utils.h>` alışkanlığı; üst-dizin BUILD_INTERFACE hilesi).
3. **Export makroları**: `VersionString, Init, Shutdown, IsInitialized, GetStatus`,
   8 katman yardımcı fonksiyonu, `Map` sınıfı ve `MapFactory` → `PRIME_MAP_EXPORT`.
4. **`MapFactory` (YENİ)** — ekibin `DBManagerFactory` deseni: private kurucu,
   `static CreateMap(const OpenConfig&)`. Tek-implementasyonlu ekibin fabrikası
   gibi **somut** sınıf döndürür (ekipte arayüz yalnız çoklu Builder'da var).
   `Map::Open` geriye uyumluluk için durur ve fabrikaya yönlendirir.
5. **CMakeLists.txt**: `add_library(prime_map SHARED ...)` (STATIC'ten geçiş);
   `PRIME_MAP_LIBRARY` tanımı; `XPLM_64.lib + opengl32 + gdi32 + user32` linki
   (DLL'de semboller çözülmeli — XPLM sembollerini X-Plane.exe yüklerken çözer,
   win.xpl ile aynı yöntem); PUBLIC include `${repo}/..` (klasör-adı önekli include);
   **install kuralları ve SDK paketi kaldırıldı**; `PRIME_MAP_DYNAMIC_CRT` kaldırıldı.
6. **CRT: `/MT` → `/MD` sabit** (ekip standardı; Qt'süz olsak da ekiple aynı).
   STL tipleri DLL sınırından geçtiği için /MD aynı zamanda doğruluk gereğidir.
7. `map_tests` / `map_preview` / `dsf_info` aynı kaldı (bkz. farklar).

### Tüketiciler (üçü): `prime_map_plugin`, `plugin-api-test`, `examples/map_api_test`
- `-DPRIME_MAP_SDK` + sabit `.lib` yolu **kaldırıldı** →
  `add_subdirectory(${PRIME_MAP_DIR} prime_map_build)` + `target_link_libraries(... prime_map)`.
- `/MD`'ye geçildi; CRT seçenekleri silindi.
- Include'lar `#include "PrimeMap.h"` → `#include <prime_map_api/PrimeMap.h>`.
- Deploy: plugin POST_BUILD artık `win.xpl` + `prime_map.dll`'i birlikte
  X-Plane'e kopyalar (build klasörüne de kopyalanır).

## Doğrulama sonuçları
- `map_tests`: **30.659 kontrol, 0 hata**.
- `prime_map.dll` + `win.xpl` derlendi; `dumpbin /dependents`:
  `win.xpl → prime_map.dll`, `prime_map.dll → XPLM_64.dll, OPENGL32, MSVCP140/VCRUNTIME140 (/MD ✓)`.
- X-Plane'e kuruldu: `Resources/plugins/prime-map-plugin/64/{win.xpl, prime_map.dll}` (17:13).
- `plugin-api-test` ve `examples/map_api_test` derlendi; her ikisinin çıktısının
  yanına `prime_map.dll` otomatik kopyalanıyor.
- Dokümanlar güncellendi + PDF'ler yeniden üretildi: `PrimeMap_API.pdf` (301 KB),
  `MIMARI.pdf`; `CHANGELOG.md`'ye 0.22 eklendi; 3 README + AGENTS.md güncellendi.

## Ekip standardından bilinçli farklar (dokümante)
| # | Fark | Neden |
|---|---|---|
| 1 | **Qt yok** | Senin kararın: kütüphane hem QML'li Qt pluginlerden hem saf C++ pluginlerden kullanılacak; X-Plane sürecine Qt DLL'i çekmek istenmez |
| 2 | **Süperrepo dışı** (`add_subdirectory` kardeş klasörden) | Senin kararın; ileride prime-map submodule slotuna taşınırsa CMake zaten hazır (`CMAKE_CURRENT_SOURCE_DIR` kullanılıyor) |
| 3 | **`namespace prime`** (ekip: `prime`) | Senin kararın |
| 4 | **C++20** (ekip: 17) | `std::format` vs. kullanılıyor; 17'ye inmek büyük refactor; süperprojede olmadığımız için onları etkilemez |
| 5 | **`/W4 /utf-8 /permissive-`** (ekipte uyarı bayrağı yok) | Mevcut kalite standardı; uyumsuzluk yaratmaz |
| 6 | **Fabrika metot adı `CreateMap`** (ekip: `createXxx` camelCase) | Bizim API'nin tüm metotları PascalCase; tek üslup |
| 7 | **`unique_ptr<Map>`** fabrika dönüşü (ekip: raw pointer + QObject parent) | Qt'süz RAII; silme DLL içindeki sanal yıkıcıdan geçer, güvenli |
| 8 | **PIMPL'li somut `Map`, soyut arayüz yok** | Ekibin tek-implementasyonlu `DBManagerFactory`'si de somut sınıf döndürür; ~80 metodun vtable'a taşınması gereksiz ABI kırılması olurdu |
| 9 | **`map_tests`/`map_preview` kaynakla derlenir** (DLL'i linklemez) | Testler iç sınıfları (LayerManager, MapView...) test eder; onlar DLL dışına export edilmiyor |
| 10 | **Genel başlık kökte ama `src/` iç ağaç korunuyor** (ekipte kaynaklar repo kökünde) | 40+ iç dosyalı yerleşim bilinçli; sadece genel başlık köke alındı |
| 11 | **Debug postfix `d` yok, çıktı `prime_map.dll`** | Ekipte set_target_properties hiç yok — birebir uyum sağlandı (önceki `prime_map_MT[d].lib` adı kalktı) |
| 12 | **COTS disiplini** (`C:\workspace\cots`) | Ekibin COTS mekanizması yok; bizim mevcut kural korundu (senin önceki kararın) |

## Yeni riskler / sabah kontrol listesi
1. **X-Plane runtime testi YAPILMADI** (win.xpl + prime_map.dll kuruldu; senin onayınla çalıştır).
   Özellikle: DLL'in X-Plane sürecine yüklenmesi (XPLM sembol çözümü) saha testi ister.
2. **`/MD` dağıtım bedeli**: hedef makinede VC++ 2022 redistributable gerekir (eski /MT'te gerekmezti).
   IOS istasyonlarına kurulumda vcredist paketlenmeli.
3. **Süreç-başına tek durum**: aynı X-Plane sürecinde iki host eklenti (örn. gelecekteki GNS 430 + map)
   artık aynı `gData`'yı paylaşır → ikinci `Init` false döner. Statik modelde her host kendi
   kopyasını taşıyordu. Dokümana işlendi; GNS 430 senaryosu düşünülüyorsa mentora danış.
4. **`prime_map_sdk` klasörü** eski paketiyle duruyor; hiçbir şey artık onu kullanmıyor.
   Onayına göre silebilirim.
5. Eski Qt Creator derleme klasörleri (`build/Desktop_...`) bayat cache ile açılır;
   Qt Creator'da sadece Run CMake yeterli (self-heal blokları korundu).
6. Bitbucket tarafı (senin işin): repo oluşturma/push; `prime-map.git` erişimi varsa
   süperprojeye submodule olarak eklenme yolu da hazır (`CMAKE_CURRENT_SOURCE_DIR` uyumlu).

## Derleme komutları (gece kullandığım)
```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
set PATH=C:\Qt\Tools\Ninja;%PATH%
cmake -S C:\workspace\projects\prime_map_api -B ...\build\api22 -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl ^
  -DMAP_TESTS=ON -DMAP_PREVIEW=ON
cmake --build ...\build\api22      &  ...\build\api22\map_tests.exe
cmake -S prime_map_plugin -B ...\build\p22 ...   (deploy dahil)
```
Not: Yeni derleme klasörleri kullandım (api22, p22, t22, e22); senin `build/` Qt Creator
klasörlerine dokunulmadı.
