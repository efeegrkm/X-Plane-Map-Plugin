# Prime XMap API

X-Plane 11 eklentilerine gömülen katmanlı harita kütüphanesi. Sürüm 1.0.0.

Harita yalnızca o bilgisayardaki X-Plane verisini kullanır: zemin ve yollar DSF sahne dosyalarından, havalimanları `apt.dat` dosyalarından, navaid'ler X-Plane nav veritabanından, hava yolları `earth_awy.dat` dosyasından, uçak ve rota dataref'lerden ve FMS'ten gelir. Host eklenti haritayı bir OpenGL dokusu olarak alır ve kendi penceresinde ya da panelinde istediği yere basar.

## Belgeler

| Belge | İçerik |
|---|---|
| `docs/PrimeMap_API.pdf` | Kullanım kılavuzu ve API başvurusu |
| `docs/MIMARI.pdf` | Kodun mimarisi |
| `docs/SRS.md` | Yazılım gereksinimleri ve durumları |
| `docs/CHANGELOG.md` | Sürüm notları |

## Kullanım

```cmake
add_subdirectory(<prime_map_api klasörü> prime_map_build)
target_link_libraries(my_plugin PRIVATE prime-map-api)
```

```cpp
#include <prime_map_api/PrimeMap.h>

prime::XMapManager* tManager = prime::XMapManagerFactory::createXMapManager();
tManager->setDataDirectory(myPluginFolder);
tManager->initialize();

prime::XMapConfig tConfig;
tConfig.mName = "main";
prime::XMap* tMap = tManager->addXMap(tConfig);
tMap->setTextureSize(600, 400);
tMap->initialize();
```

Host her karede `renderTexture()` çağırır ve `getTextureId()` dokusunu basar; girdileri `injectMouse`, `injectMouseMove`, `injectWheel` ve `injectKey` ile iletir. `prime_map.dll` host'un `win.xpl` dosyasıyla aynı klasörde durur. Host C++20 ve `/MD` çalışma zamanı ile derlenmelidir. Eksiksiz bir örnek `examples/texture_host` klasöründedir.

## Derleme

Gerekenler: Visual Studio 2022 veya sonrası, CMake 3.16+, Ninja ve üçüncü taraf bileşenler.

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DMAP_TESTS=ON -DMAP_PREVIEW=ON
cmake --build build
build\map_tests.exe
```

| Seçenek | Anlamı |
|---|---|
| `MAP_TESTS` | X-Plane'siz birim testleri (`map_tests`) |
| `MAP_PREVIEW` | `map_preview` ve `dsf_info` araçları |
| `COTS_DIR`, `XPSDK` | Üçüncü taraf bileşen klasörü ve X-Plane SDK klasörü |

Sürüm numarası `src/api/XMapTypes.h` ve `CMakeLists.txt` içinde birlikte tutulur; CMake ikisinin aynı olduğunu denetler.

## Üçüncü taraf bileşenler

Üçüncü taraf kodu depoda tutulmaz. CMake bileşenleri sırasıyla `-DCOTS_DIR`, `C:\workspace\cots` ve kardeş `..\cots` klasöründe arar.

| Bileşen | Sürüm | Klasör |
|---|---|---|
| X-Plane SDK | 3.0 (XPLM301) | `cots/X-Plane-SDK` |
| LZMA SDK | 24.05 veya sonrası | `cots/lzma` |
| earcut.hpp | 2.2.4 | `cots/earcut/mapbox/earcut.hpp` |
| stb_truetype.h | 1.26 veya sonrası | `cots/stb/stb_truetype.h` |

LZMA, earcut ve stb yalnızca kütüphane derlenirken gerekir. Host yalnızca X-Plane SDK'ya ihtiyaç duyar.

## Araçlar

`map_preview` kütüphanenin katman kodunu gerçek X-Plane verisiyle bir resim dosyasına çizer:

```bat
map_preview --lat 47.45 --lon -122.30 --zoom 11 --mode dark --cache <klasör>\ --out ksea.ppm
```

Diğer seçenekler: `--show`, `--hide`, `--only`, `--layer-zoom`, `--panel`, `--describe x,y`, `--demo-drawings`, `--terrain-colors`, `--bench`.

`dsf_info` bir DSF dosyasının içeriğini döker: özellikler, dışlama alanları, arazi türleri, üçgen sayıları, yol ve poligon envanteri.
