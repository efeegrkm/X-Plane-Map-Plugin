# plugin_api_test

Prime XMap API'yi (0.26) denemek için ayrı bir X-Plane 11 eklentisi: kendi UI penceresi (X-Plane çerçevesi:
**başlıktan taşınır, kenarlardan boyutlanır**) + içine gömülü harita. Bütün API çağrıları
`src/XMapApiCalls.cpp` içindedir; liste: `docs/API_LISTESI.md`.

Harita `WindowStyle::Texture` ile açılır: **ayrı bir pencere yoktur**. Kütüphane haritayı bir openGL
dokusuna çizer (`renderTexture`), eklenti onu kendi penceresindeki harita alanına basar. Pencere taşınınca /
boyutlanınca harita aynı karede onunla gelir. Haritanın kendi arayüzü ve faresi açıktır: sürükleme, tekerlek,
sağ tık, katman paneli, "Go to airport" (klavye) eklentiden `inject*` ile iletilir. Ayarlar:
`maps/ui_test_texture/`.

| Dosya | Ne |
|---|---|
| `src/XMapUiTestMain.cpp` | X-Plane giriş noktaları, pencere, düğmeler (pan / zoom) |
| `src/XMapApiCalls.h/.cpp` | Her metot tek bir API çağrısı |
| `src/MultiXMapSelfTest.h/.cpp` | Çoklu harita / fabrika / ayar dosyası otomatik testi (X-Plane içinde) |

**Veri klasörü:** bu eklenti kütüphaneye kendi klasörünü verir
(`<X-Plane>\Resources\plugins\plugin-api-test\`): `prime_map.json`, `maps/`, `cache/` orada oluşur.
Böylece resmî eklentinin `%LOCALAPPDATA%\prime\prime-xmap\` klasörüyle çakışmaz (iki ayrı zemin işçisi
aynı önbellek dosyasını yazmaz). İlk açılışta kendi zemin önbelleğini kurar.

Kütüphane tüketimi: `prime_map_api` aynı CMake oturumuna `add_subdirectory` ile katılır, hedef adına
(`prime-map-api`) linklenir; SDK paketi yoktur. Kütüphane bu projenin kardeşi olmalı (`../prime_map_api`);
farklı yerdeyse `-DPRIME_MAP_DIR=<klasor>`. COTS bileşenleri ve adım adım akış: `../prime_map_api/README.md`.

## Çoklu harita otomatik testi

`src/MultiXMapSelfTest.cpp`, fabrika / bağımsız harita / ayar dosyası / ortak önbellek sınırı özelliklerini
X-Plane **içinde** dener. Eklenti klasörüne (`<X-Plane>\Resources\plugins\plugin-api-test\`) boş bir
`selftest.flag` dosyası koyun ve X-Plane'i açın; Log.txt'de `[plugin-api-test] SELFTEST PASS/FAIL ...`
satırları ve sonunda `SELFTEST DONE: N passed, M failed` çıkar. Dosyayı silince test çalışmaz.

## Derleme ve kurulum

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build -G Ninja -DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe -DCMAKE_BUILD_TYPE=Release -DMAP_DEPLOY=ON
ninja -C build
```

`-DMAP_DEPLOY=ON` (varsayılan kapalı) derleme sonrası `win.xpl` + `prime_map.dll`'yi
`<X-Plane>\Resources\plugins\plugin-api-test\64\` altına kopyalar (X-Plane kapalıyken; açıkken dosya kilitli).
Resmî eklentiyle aynı süreçte çalışırken iki eklentinin `prime_map.dll`'si **aynı sürüm** olmalı
(Windows aynı adlı DLL'i bir kez yükler); ikisini birlikte derleyip kurun.

## Üçüncü Taraf Bileşenler

Şirket kuralı gereği üçüncü taraf bileşenler repoda tutulmaz; `C:\workspace\cots` (yoksa kardeş `..\cots`)
altından okunur:

| Bileşen | Sürüm | Ne için kullanılıyor | İndirme | Kurulum klasörü |
|---|---|---|---|---|
| X-Plane SDK | 3.0 (XPLM301) | X-Plane eklenti arayüzü: başlıklar + `XPLM_64.lib` | https://developer.x-plane.com/sdk/ | `C:\workspace\cots\X-Plane-SDK` |
