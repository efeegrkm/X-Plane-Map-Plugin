# Prime XMap 1.1.0

Üç kardeş proje:

| Klasör | İçerik |
|---|---|
| `prime_map_api/` | Harita kütüphanesi ve genel API (`prime_map.dll`, CMake hedefi `prime-map-api`) |
| `prime_map_plugin/` | Resmî X-Plane eklentisi |
| `plugin-api-test/` | API'yi X-Plane içinde deneyen test eklentisi |

Belgeler `prime_map_api/docs/` klasöründedir: kullanım kılavuzu ve API başvurusu (`PrimeMap_API.pdf`), mimari (`MIMARI.pdf`), gereksinimler (`SRS.md`) ve sürüm notları (`CHANGELOG.md`).

Üçüncü taraf bileşenler (X-Plane SDK, LZMA SDK, earcut, stb_truetype) depoda tutulmaz; `C:\workspace\cots` ya da kardeş `cots` klasöründen okunur.
