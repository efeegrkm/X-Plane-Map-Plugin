# X-Plane Map Plugin — Prime XMap 0.26

Üç proje (kardeş klasörler):

```
prime_map_api/     harita kütüphanesi + genel API (CMake hedefi prime-map-api -> prime_map.dll)
prime_map_plugin/  resmî X-Plane eklentisi (ince kabuk; haritaya sadece API ile erişir; şu an LEGACY Standalone)
plugin-api-test/   API'yi deneyen test eklentisi: pencere içinde doku harita + çoklu harita otomatik testi
```

API: `prime::XMapManagerFactory::createXMapManager()` → `XMapManager::initialize()` → `addXMap()` → ayarla →
`XMap::initialize()`. Hedef ve önerilen yol **`WindowStyle::Texture`**: harita bir openGL dokusuna (framebuffer) çizilir,
host onu istediği yere basar, girdiyi `inject*` ile iletir. `Standalone` / `Embedded` LEGACY. Ayarlar yalnızca JSON
(`prime_map.json`, `maps/<ad>/map_settings.json`, `terrain_colors.json`); geliştirme aşamasında geriye uyumluluk yok.

| Belge | Ne |
|---|---|
| `prime_map_api/docs/PrimeMap_API.pdf` | Kullanım kılavuzu ve referans: hızlı başlangıç, sıfırdan entegrasyon, bütün fonksiyonlar, özelleştirme, dosyalar |
| `prime_map_api/docs/SRS.md` | Yazılım gereksinimleri (IEEE 29148 yapısı), her gereksinimin bugünkü durumu |
| `prime_map_api/docs/MIMARI.pdf` | Mimari |
| `prime_map_api/README.md` | Derleme, testler, araçlar, üçüncü taraf bileşenler |
| `prime_map_api/examples/texture_host/` | En küçük tam host eklentisi (derlenir) |

Tüketim: `add_subdirectory(<prime_map_api yolu> prime_map_build)` + `target_link_libraries(<hedef> PRIVATE prime-map-api)`;
`#include <prime_map_api/PrimeMap.h>`; `prime_map.dll` host'un `win.xpl` dosyasının yanına kopyalanır. CRT `/MD`.
Üçüncü taraf bileşenler (X-Plane SDK, LZMA, earcut, stb) repoda tutulmaz: `C:\workspace\cots` ya da kardeş `cots` klasörü.
