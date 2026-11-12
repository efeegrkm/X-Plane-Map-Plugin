# plugin_api_test

Prime Map API'yi denemek için ayrı bir X-Plane 11 eklentisi: kendi UI'ı +
harita gömme testi (Adım 1 UI, Adım 2 API bağlama).
Bütün API çağrıları `src/MapApiCalls.cpp` içindedir; liste: `docs/API_LISTESI.md`.

Kütüphane tüketimi: `prime_map_api` aynı CMake oturumuna
`add_subdirectory` ile katılır, hedef adına (`prime_map`) linklenir; SDK paketi yoktur.
**Ön koşul:** kütüphane reposu bu projenin kardeşi olarak klonlanmalıdır
(`../prime_map_api`); farklı yerdeyse `-DPRIME_MAP_DIR=<klasor>`. Kütüphane
derlenirken gerekli COTS bileşenleri (X-Plane SDK, LZMA, earcut, stb) ve
adım adım akış: `../prime_map_api/README.md`.
Derleme sonrası `prime_map.dll` otomatik olarak `win.xpl`'nin yanına kopyalanır;
X-Plane'e kurarken ikisini de `<X-Plane>\Resources\plugins\plugin-api-test\64\`
altına koyun.

## 0.23: çoklu harita otomatik testi

`src/MultiMapSelfTest.cpp`, fabrika / bağımsız harita / ayar dosyası / ortak önbellek sınırı
özelliklerini X-Plane **içinde** kendi kendine dener. Çalıştırmak için eklenti klasörüne
(`<X-Plane>\Resources\plugins\plugin-api-test\`) boş bir `selftest.flag` dosyası koyun ve
X-Plane'i açın; Log.txt'de `[plugin-api-test] SELFTEST PASS/FAIL ...` satırları ve sonunda
`SELFTEST DONE: N passed, M failed` çıkar. Dosyayı silince test çalışmaz. Aynı süreçte resmî
eklenti de kuruluysa kütüphaneyi ilk başlatan host'un klasörü veri klasörü olur; bayrak dosyasını
o klasöre koyun (Log'daki `Prime Map API ... ready (data: ...)` satırı hangisi olduğunu söyler).

Kurulum: `-DMAP_DEPLOY=ON` ile derleme sonrası `win.xpl` + `prime_map.dll` X-Plane'e kopyalanır
(varsayılan kapalı).

## Derleme

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build -G Ninja -DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe -DCMAKE_BUILD_TYPE=Release
ninja -C build   ->  build/plugin_api_test/64/win.xpl
```

`win.xpl`'yi X-Plane'e kurmak için: `<X-Plane>\Resources\plugins\plugin-api-test\64\`
(X-Plane kapalıyken).

## Üçüncü Taraf Bileşenler

Şirket kuralı gereği üçüncü taraf bileşenler repoda tutulmaz;
`C:\workspace\cots` altından okunur:

| Bileşen | Sürüm | Ne için kullanılıyor | İndirme | Kurulum klasörü |
|---|---|---|---|---|
| X-Plane SDK | 3.0 (XPLM301) | X-Plane eklenti arayüzü: başlıklar + `XPLM_64.lib` | https://developer.x-plane.com/sdk/ | `C:\workspace\cots\X-Plane-SDK` |

Varsayılan kütüphane yolu: kardeş klasör `../prime_map_api`;
farklı yerdeyse `-DPRIME_MAP_DIR=<klasor>`.
