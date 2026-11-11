# Prime Map API — CHANGELOG

## 0.22 (ekip kalıbına geçiş, 2026-11-11)

Kütüphane sunumu ekibin kalıbına (prime-utils / prime-dbmanager / prime-builder)
çevrildi. API fonksiyonları değişmedi (tek ek: `MapLibrary` sınıfı).

- `prime_map` STATIC → **SHARED DLL** (`prime_map.dll` + import `prime_map.lib`).
- `primemapglobal.h`: `PRIME_MAP_EXPORT` makrosu (ekibin `utilsglobal.h` deseni).
- **Namespace `prime`**: tüm genel API `prime` namespace'ine taşındı (ekibin
  namespace alışkanlığı; `PrimeMap::` → `prime::`).
- **`MapLibrary`** (kütüphane kendisi bir nesne; mentor geri bildirimi):
  `MapLibrary::Get().Init(const InitConfig&)` / `Shutdown()` / `IsInitialized()` /
  `GetStatus()` / `CreateMap(const OpenConfig&)`. Serbest `Init`/`Shutdown`
  fonksiyonları kaldırıldı. `Map::Open` geriye uyumluluk için durur ve
  `CreateMap`'e yönlendirir.
- **Çoklu host**: SHARED DLL'de aynı X-Plane sürecinde birden çok host eklenti
  olabilir; `Init`/`Shutdown` referans sayaçlı (ilk host'un `InitConfig`'i geçerli,
  son host çıkınca tam kapanma).
- **Çoklu harita**: "tek harita" kuralı kaldırıldı; aynı anda birden çok açık
  `Map` penceresi desteklenir (`MapData` tüm açık pencereleri besler).
- Genel başlık repo köküne taşındı: `#include <prime_map_api/PrimeMap.h>` (ekibin
  klasör-adı önekli include alışkanlığı).
- CRT: `/MT` → **`/MD` sabit** (ekip standardı; `PRIME_MAP_DYNAMIC_CRT` kaldırıldı).
  DLL sınırından STL tipleri geçtiği için host da `/MD` olmak zorundadır.
  Hedef makinede MSVC 2022 redistributable gerekir.
- SDK paketi ve `install` kuralları **kaldırıldı** (ekipte paket yok). Tüketim:
  `add_subdirectory(<prime_map_api>)` + `target_link_libraries(... prime_map)`.
  Tüketiciler güncellendi: `prime_map_plugin`, `plugin-api-test`, `examples/map_api_test`.
- Dağıtım: `prime_map.dll` host eklentinin (`win.xpl`) yanına kopyalanır;
  `prime_map_plugin` derlemesi ikisini de X-Plane'e kurar.
- Windows sürüm kaynağı (`primemap.rc.in` → `prime_map.dll`, `prime_map_plugin.rc.in` →
  `win.xpl`): CMake `PROJECT_VERSION` dosya özelliklerine gömülür (Ayrintilar sekmesi).
- **Sürüm birleştirildi:** `PrimeMap.h`'deki `kVersionMajor/Minor/Patch` (ve
  `VersionString()`) 1.0.0 → **0.22.0**; artık CMake `project(VERSION)` ile aynıdır ve
  configure sırasında denetlenir (uyusmazlıkta configure hata verir).
- Dokümantasyon yeni tüketim modeline göre adım adım yenilendi: README "kendi
  eklentine kurma" rehberi, `prime_map_plugin` README'sinde iki-repo kurulumu ve
  çalıştırma adımları, `plugin-api-test` README'si, bu belge (Adım 2'de
  `PRIME_MAP_DIR` denetimi, POST_BUILD kopyası, VC_redist, sorun giderme tablosu).
- **Runway zoom eşiği pavement ile eşitlendi:** pistler eskiden zoom 9'dan itibaren
  çiziliyordu; kaplamalar (pavement) 11'de göründüğünden uzak zoom'da pistler havada
  kalıyordu. Artık pistler de zoom 11'de görünür (kaplama kaybolunca pist de kaybolur).
- Dev fark (bilinçli): `map_tests` / `map_preview` kütüphane kaynaklarını hâlâ
  doğrudan derler (iç sınıflar DLL dışına export edilmez, testler iç sınıfları
  test eder). Testler: 30.659 kontrol, 0 hata.

## 1.0.0 (prime-map-plugin 0.20, 2026-11-08)

İlk sürüm. Örnek/test eklentisi: `examples/map_api_test`.

- Tek genel başlık `PrimeMap.h`; statik kütüphane `prime_map_MT[d].lib` / `prime_map_MD[d].lib` (LZMA içinde).
- `Init` / `Shutdown` / `GetStatus` / `VersionString`.
- `Map::Open` (tek harita; 0.22'de çoklu haritaya çevrildi), `Embedded` ve `Standalone` pencere, UI ve fare anahtarları
  (fare kapalıyken tıklama alttaki pencereye geçer).
- Görünüm: merkez, zoom, zoom/pan adımları, `PanBy`, `FitBounds`, takip, `GoToAirport`, `FindAirports`,
  `ScreenToGeo` / `GeoToScreen`.
- Modlar, `MapLayer` enum'u ve panel kuralları, kullanıcı çizimleri, uçak/rota kaynağı (X-Plane ya da host),
  sorgu (`QueryAt`), ayar ve zemin ayarı dışa/içe aktarma (+ revizyon sayaçları), ön önbellek, teşhis.
- Tasarım taslağına göre eklenenler: `SetPoppedOut`, `GetSettingsRevision`, `GetDrawingsRevision`, `KindOf`,
  `TopLevelLayers`, `IsLayerDefaultVisible`, `MinZoom`/`MaxZoom`, `IsInitialized`, `Get...Source`.
  `ParentOf` `std::optional` döner. `[cache]` ayarları `InitConfig`'e, `[render]`/`[text]` `OpenConfig`'e taşındı.
