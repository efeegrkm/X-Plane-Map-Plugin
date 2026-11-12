# X-Plane Map Plugin — 0.23

Üç bağımsız proje (kardeş klasörler):

```
prime_map_api/     harita kütüphanesi + genel API (prime_map SHARED → prime_map.dll)
prime_map_plugin/  resmî X-Plane eklentisi (ince kabuk; haritaya sadece API ile erişir)
plugin-api-test/   API'yi ayrı bir eklentide deneyen test eklentisi (+ çoklu harita otomatik testi)
```

0.23: harita **fabrikası** (`MapLibrary::CreateMap()` → ayarla → `Open()`), tamamen **bağımsız çoklu
harita**, harita başına ayar dosyaları (`maps/<ad>/`), merkezi `prime_map.ini`, bütün haritalar için
**ortak önbellek** (sınırlar açık harita sayısıyla büyür).

- API referansı (bütün fonksiyonlar, dosyalar, örnekler): `prime_map_api/docs/PrimeMap_API.pdf`
- Mimari: `prime_map_api/docs/MIMARI.pdf` · Sürüm notları: `prime_map_api/docs/CHANGELOG.md`
- Asistanlar için durum / kurallar: `prime_map_api/AGENTS.md`
- Zemin yükleme hızı araştırması: `prime_map_api/docs/ZEMIN_HIZ_RAPORU.md`

Tüketim: `add_subdirectory(<prime_map_api yolu> prime_map_build)` +
`target_link_libraries(<hedef> PRIVATE prime_map)`; `#include <prime_map_api/PrimeMap.h>`;
`prime_map.dll` host'un `win.xpl` dosyasının yanına kopyalanır. CRT `/MD`.
Üçüncü taraf bileşenler (X-Plane SDK, LZMA, earcut, stb) repoda tutulmaz: `C:\workspace\cots`
ya da kardeş `cots` klasörü (`prime_map_api/README.md`).
