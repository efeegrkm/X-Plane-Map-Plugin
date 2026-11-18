---
title: Prime XMap API
subtitle: Kullanım Kılavuzu ve Başvuru
version: 1.0.0
date: 2026-11-18
---

# Genel bakış

Prime XMap API, bir X-Plane 11 eklentisinin kendi ekranına katmanlı ve etkileşimli bir harita yerleştirmesini sağlayan bir C++ kütüphanesidir. Kütüphane `prime_map.dll` olarak derlenir ve host eklentiyle aynı klasörde çalışır.

Harita yalnızca o bilgisayardaki X-Plane kurulumunun verisini kullanır: DSF sahne dosyalarından zemin, su, kabartma ve yollar; `apt.dat` dosyalarından havalimanları; X-Plane nav veritabanından VOR, NDB, ILS ve fix'ler; `earth_awy.dat` dosyasından hava yolları; dataref'lerden uçak konumu ve FMS'ten uçuş planı. Dış servis, internet bağlantısı veya ek veri paketi gerekmez. Simülatörde hangi sahne kuruluysa haritada o görünür.

## Neler yapılabilir

- Haritayı host'un istediği yere ve boyuta, bir OpenGL dokusu olarak yerleştirmek.
- Birbirinden bağımsız, istenen sayıda harita açmak.
- Beş harita modundan birini seçmek: Scenery, VFR, IFR Low, IFR High, Dark.
- 56 katman ve alt katmanın her birini mod başına açıp kapatmak ve her katmanın hangi zoom aralığında görüneceğini belirlemek.
- 15 katmanın rengini değiştirmek; zeminin renklerini, kabartmasını, su görünümünü ve VFR yükseklik bantlarını ayarlamak.
- Uçak konumunu ve rotayı X-Plane'den ya da host'tan almak.
- Haritaya nokta, rota ve bölge çizmek; çizimleri okumak, kaydetmek ve geri yüklemek.
- Ekran ile coğrafi koordinat arasında dönüşüm yapmak ve imlecin altındaki nesneyi sorgulamak.
- Ayarları dosyada tutmak ya da metin olarak dışa ve içe aktarmak.
- Bir bölgenin zemin verisini önceden hazırlamak.

## Temel bilgiler

| Konu | Değer |
|---|---|
| Platform | X-Plane 11.50 ve üstü, Windows x64, X-Plane SDK 3.0 (XPLM301). X-Plane 12 bu sürümün kapsamında değildir. |
| Dil ve derleyici | C++20, Microsoft Visual C++ (Visual Studio 2022 ve sonrası), CRT `/MD` |
| Başlık ve ad alanı | `#include <prime_map_api/PrimeMap.h>`, `namespace prime` |
| CMake hedefi | `prime-map-api`; çıktı `prime_map.dll` ve `prime_map.lib` |
| Giriş noktası | `XMapManagerFactory::createXMapManager()` → `XMapManager` → `addXMap()` → `XMap` |
| Görüntüleme | `WindowStyle::Texture`: harita, host'un istediği yere bastığı bir dokuya çizilir. |
| Thread | Bütün API çağrıları X-Plane ana thread'inden yapılır. Veri yükleme ve zemin üretimi kütüphanenin kendi arka plan thread'lerinde çalışır. |
| Hata modeli | İstisna kullanılmaz. Geçersiz bir çağrı hiçbir şeyi değiştirmez, `Log.txt`'ye tek satır `WARNING` yazar ve `false`, boş değer ya da varsayılan değer döner. |

## Üç temel nesne

**Yönetici (`XMapManager`)** veriyi yükler ve haritaların sahibidir. Nav verisi, havalimanı indeksi, hava yolları, zemin üreticisi ve önbellek bir yöneticinin bütün haritaları arasında ortaktır.

**Harita (`XMap`)** tek bir haritadır. Görünümü, modu, katman durumları, renkleri, çizimleri, veri kaynakları ve ayar dosyaları kendisine aittir.

**Doku** haritanın çizildiği OpenGL dokusudur. Host her karede `renderTexture()` çağırır ve `getTextureId()` ile aldığı dokuyu kendi penceresinde ya da panelinde istediği dörtgene basar. Fare ve klavye girdileri `inject*` fonksiyonlarıyla haritaya iletilir.

# Hızlı başlangıç

Bu bölüm, çalışan bir X-Plane eklentisine haritayı eklemek için gereken çağrıları sırasıyla gösterir. Derlenmiş, eksiksiz bir örnek depoda `prime_map_api/examples/texture_host` klasöründedir; yeni bir eklentiye başlarken o projeyi kopyalamak en hızlı yoldur.

## Projeye ekleme

```cmake
add_subdirectory(${CMAKE_SOURCE_DIR}/../prime_map_api prime_map_build)
target_link_libraries(my_plugin PRIVATE prime-map-api)
add_custom_command(TARGET my_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:prime-map-api> $<TARGET_FILE_DIR:my_plugin>)
```

İlk satır kütüphaneyi host ile aynı CMake oturumunda derler, ikinci satır başlık klasörünü ve bağlantıyı ekler, üçüncü satır `prime_map.dll` dosyasını `win.xpl`'nin yanına kopyalar. Host da `/MD` çalışma zamanı ile ve C++20 ile derlenmelidir:

```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
```

## Açılış

`XPluginEnable` içinde yönetici kurulur ve harita açılır:

```cpp
#include <prime_map_api/PrimeMap.h>

prime::XMapManager* gManager = prime::XMapManagerFactory::createXMapManager();
gManager->setDataDirectory(myPluginFolder);
gManager->initialize();

prime::XMapConfig tConfig;
tConfig.mName = "main";
tConfig.mUiVisible = true;
tConfig.mMouseInput = true;
prime::XMap* gMap = gManager->addXMap(tConfig);
gMap->setTextureSize(600, 400);
gMap->initialize();
```

`setDataDirectory` ayar ve önbellek dosyalarının nereye yazılacağını belirler; her host kendi eklenti klasörünü vermelidir. `XMapConfig` varsayılan olarak Texture yolunu ve Dark modu kullanır. Harita, `initialize()` çağrılana kadar açılmaz.

## Çizim

Host penceresinin draw callback'inde:

```cpp
gMap->setTextureSize(mapWidth, mapHeight);
if (gMap->renderTexture() && gMap->getTextureId() != 0)
{
    XPLMSetGraphicsState(0, 1, 0, 0, 0, 0, 0);
    XPLMBindTexture2d(static_cast<int>(gMap->getTextureId()), 0);
    // (0,0) sol-alt, (1,1) sag-ust doku koordinatlariyla dortgen cizilir
}
```

`renderTexture()` her karede çağrılabilir; bir şey değişmediyse yeniden çizmez ve mevcut doku geçerli kalır.

## Girdi

Host'un fare ve klavye callback'lerinde koordinatlar harita alanına göre çevrilip iletilir. Harita alanının sol-alt köşesi `(0, 0)`'dır ve birim boxel'dir.

```cpp
gMap->injectMouse(x - mapLeft, y - mapBottom, prime::MouseAction::Down);
gMap->injectMouseMove(x - mapLeft, y - mapBottom);
gMap->injectWheel(x - mapLeft, y - mapBottom, clicks);
if (gMap->wantsKeyboard())
{
    XPLMTakeKeyboardFocus(myWindow);
}
```

## Kapanış

`XPluginDisable` içinde sırasıyla harita kaldırılır, yönetici durdurulur ve silinir:

```cpp
gManager->removeXMap(gMap);
gManager->shutdown();
prime::XMapManagerFactory::destroyXMapManager(gManager);
```

## Kurulum ve ilk kontrol

Eklenti klasörü şu düzende olmalıdır:

```text
<X-Plane 11>/Resources/plugins/<eklenti>/
    64/win.xpl
    64/prime_map.dll
```

X-Plane açıldığında `Log.txt` içinde şu satır görünür:

```text
Prime Map API 1.0.0 ready (data: ..., cache: ..., dll: ...\64\prime_map.dll)
```

İlk açılışta havalimanı indeksi birkaç saniye ile yarım dakika arasında oluşturulur; sonraki açılışlarda önbellekten okunur.

# Derleme ve kurulum

## Ön koşullar

| Gereken | Açıklama |
|---|---|
| X-Plane 11.50 veya üstü | Windows x64. Varsayılan kurulum klasörü `C:\X-Plane 11`. |
| Visual Studio 2022 veya sonrası | "Desktop development with C++" iş yükü. Derleme `vcvars64.bat` ortamında yapılır. |
| CMake 3.16+ ve Ninja | Qt Creator kuruluysa `C:\Qt\Tools\CMake_64` ve `C:\Qt\Tools\Ninja` kullanılabilir; Qt kütüphanesi gerekmez. |
| Üçüncü taraf bileşenler | Depoda tutulmaz; aşağıdaki tabloya göre tek bir klasöre açılır. |

## Üçüncü taraf bileşenler

Bileşenler tek bir `cots` klasörüne konur. CMake bu klasörü sırasıyla `-DCOTS_DIR` değerinde, `C:\workspace\cots` altında ve `prime_map_api` klasörünün yanındaki `cots` klasöründe arar.

| Bileşen | Sürüm | Klasör | Kullanan |
|---|---|---|---|
| X-Plane SDK | 3.0 (XPLM301) | `cots/X-Plane-SDK` | kütüphane ve host |
| LZMA SDK | 24.05 veya sonrası | `cots/lzma` (SDK'nın `C` klasöründeki dosyalar) | kütüphane |
| earcut.hpp | 2.2.4 | `cots/earcut/mapbox/earcut.hpp` | kütüphane |
| stb_truetype.h | 1.26 veya sonrası | `cots/stb/stb_truetype.h` | kütüphane |

## Klasör düzeni

```text
<çalışma klasörü>/
    cots/
    prime_map_api/          kütüphane; klasör adı include önekidir, değiştirilmemelidir
    my_plugin/
        CMakeLists.txt
        src/MyPlugin.cpp
```

## Derleme

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S my_plugin -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Kütüphanenin CMake seçenekleri:

| Seçenek | Varsayılan | Anlamı |
|---|---|---|
| `COTS_DIR` | `C:/workspace/cots` | Üçüncü taraf bileşen klasörü |
| `XPSDK` | `<COTS_DIR>/X-Plane-SDK` | X-Plane SDK klasörü |
| `MAP_TESTS` | `OFF` | X-Plane'siz birim testlerini (`map_tests`) derler |
| `MAP_PREVIEW` | `OFF` | Katmanları X-Plane olmadan resme çizen `map_preview` aracını derler |

Örnek host ve resmî eklenti ayrıca `PRIME_MAP_DIR` (kütüphane klasörü), `XPLANE_DIR` ve `MAP_DEPLOY` seçeneklerini kullanır. `MAP_DEPLOY=ON` derleme sonunda `win.xpl` ve `prime_map.dll` dosyalarını X-Plane'in eklenti klasörüne kopyalar. X-Plane açıkken bu dosyalar kilitli olduğundan kopyalama yapılmadan önce X-Plane kapatılmalıdır. Başka bir bilgisayarda çalıştırmak için Visual C++ yeniden dağıtılabilir paketi (x64) kurulu olmalıdır.

## Sorun giderme

| Belirti | Neden ve çözüm |
|---|---|
| CMake "X-Plane SDK bulunamadi" diyor | `cots` klasörü eksik ya da `COTS_DIR` yanlış. |
| CMake "prime_map_api bulunamadi" diyor | `PRIME_MAP_DIR` verilmeli. Include satırı `<prime_map_api/PrimeMap.h>` biçiminde olmalı. |
| Bağlantı hatası ya da açıklanamayan çökmeler | Host `/MT` ile derleniyor. `CMAKE_MSVC_RUNTIME_LIBRARY` satırı eklenmeli. |
| X-Plane eklentiyi yüklemiyor, Log'da hata 126 | `prime_map.dll` `64` klasöründe değil ya da Visual C++ çalışma zamanı kurulu değil. |
| Log'da "host built with X, loaded DLL is Y" uyarısı | Aynı X-Plane'de başka bir eklenti farklı sürüm `prime_map.dll` getirmiş. Windows aynı adlı DLL'i bir kez yükler; bütün eklentiler aynı sürümü taşımalıdır. Uyarıdaki yol, yüklenen dosyayı gösterir. |
| `addXMap` `nullptr` döndü | `XMapManager::initialize()` çağrılmamış. |
| Doku boş ya da siyah | Veri henüz yükleniyor (`getStatus()`), ya da `renderTexture()` draw callback dışında çağrılıyor. `false` döndüğünde doku basılmamalıdır. |
| Tıklama haritaya ulaşmıyor | `mMouseInput` kapalı, koordinat harita alanına göre çevrilmemiş ya da basma haritada başladığı halde sürükleme ve bırakma iletilmiyor. |
| Havalimanı arama kutusuna yazılamıyor | `wantsKeyboard()` `true` iken host klavye odağını almalı ve tuşları `injectKey` ile iletmelidir. |
| Kodda verilen ayar açılışta uygulanmıyor | Ayar dosyasındaki değer geçerli olmuştur. Ayrıntı için "Kod ve dosya: kalıcılık" bölümüne bakın. |
| İki eklentinin haritaları birbirinin ayarlarını eziyor | İki host da varsayılan veri klasörünü kullanıyor. Her host `setDataDirectory` ile kendi klasörünü vermelidir. |

# Temel kavramlar

## Nesneler ve sahiplik

| Nesne | Oluşturma ve silme | Ne tutar |
|---|---|---|
| `XMapManager` | `XMapManagerFactory::createXMapManager()` ve `destroyXMapManager()`. Her çağrı yeni bir yönetici verir. | Ortak veri, önbellek, merkezi ayarlar ve haritalar |
| `XMap` | `XMapManager::addXMap()` ve `removeXMap()`. Yönetici silinirken kalan haritaları da siler. | Bir haritanın görünümü, modu, katmanları, renkleri, çizimleri, veri kaynakları, zemin ayarları ve dosyaları |

Yönetici ve harita nesneleri kütüphanenin içinde oluşturulup silinir; host bu nesneleri `new` ya da `delete` ile yönetmez. `removeXMap` çağrısından sonra haritanın işaretçisi geçersizdir.

## Yaşam döngüsü

| Durum | Geçiş | Yapılabilecekler |
|---|---|---|
| Yönetici oluşturuldu | `createXMapManager()` | Klasör, log ve önbellek ayarları; yol sorguları |
| Yönetici çalışıyor | `initialize()` | Harita ekleme, durum sorgusu, merkezi ayarlar |
| Harita oluşturuldu | `addXMap()` | Bütün `set` ve `get` çağrıları; değerler nesnede saklanır |
| Harita açık | `XMap::initialize()` | Bütün çağrılar; değişiklik hemen görünür ve dosyaya yazılır |
| Harita kapalı | `XMap::close()` | Oluşturulmuş durumla aynı; `initialize()` ile tekrar açılır |
| Harita kaldırıldı | `removeXMap()` | Ayarlar yazılır, nesne silinir |

`setName` ve `setStyle` yalnızca harita kapalıyken çalışır. `queryAt`, `bringToFront`, `setPoppedOut` ve `renderTexture` yalnızca harita açıkken anlamlıdır.

Yönetici `shutdown()` edildikten sonra elde kalan harita işaretçileri geçerli kalır, ancak bütün çağrılar etkisizdir: ilk çağrıda bir kez uyarı yazılır ve çağrılar `false`, boş değer ya da varsayılan değer döner. Doğru kapanış sırası `removeXMap`, `shutdown`, `destroyXMapManager` şeklindedir.

## Veri yükleme zaman çizelgesi

`XMapManager::initialize()` hiçbir veriyi beklemeden döner. Yükleme, X-Plane'in bir sonraki karesinde arka planda başlar. İlk kareye kadar beklenmesinin nedeni, eklenti etkinleştirilirken simülatörün sahne ve nav verisinin henüz hazır olmayabilmesi ve X-Plane'in açılışının bekletilmemesidir.

1. Havalimanı indeksi oluşturulur. İlk açılışta bütün `apt.dat` dosyaları taranır; sonraki açılışlarda indeks önbellekten yaklaşık 0,03 saniyede okunur.
2. Kurulu sahnenin DSF dizini çıkarılır.
3. Nav verisi X-Plane'den, her karede en fazla 4 ms kullanılarak okunur.
4. Nav verisi bittikten sonra hava yolları yüklenir.
5. Zemin dizini hazır olunca uçağın çevresindeki ve uçuş yönündeki zemin karoları önceden üretilir. Bu işlem hiç harita açılmamışken de çalışır; böylece harita ilk açıldığında zemin hazır olur. `setPrefetchTiles(0)` bu işlemi kapatır.
6. Bir harita açıldığında ekrandaki havalimanlarının ayrıntısı ve eksik zemin karoları arka planda hazırlanır.

İlerleme `XMapManager::getStatus()` ve `XMap::isDataLoaded()` ile izlenir. `isDataLoaded()`, ortak verinin hazır olduğunu ve o haritada bekleyen zemin dokusu kalmadığını bildirir.

## Görüntüleme yolları

| Stil | Durum | Açıklama |
|---|---|---|
| `Texture` | Önerilen ve varsayılan | Harita kütüphanenin framebuffer'ında bir dokuya çizilir. Host dokuyu kendi penceresinde, panelinde ya da kokpit ekranında istediği yere basar. Girdiler host'tan `inject*` ile gelir. |
| `Standalone` | Eski | Haritanın kendi X-Plane penceresi. Resmî Prime XMap eklentisi bu stili kullanır. |
| `Embedded` | Eski | Host penceresinin üzerine yerleştirilen çerçevesiz ayrı bir pencere. |

Eski stiller mevcut kullanımlar için korunmaktadır; yeni host kodu `Texture` kullanmalıdır. İleride kaldırılacaklardır.

## Koordinatlar ve birimler

| Büyüklük | Birim |
|---|---|
| Harita alanı (Texture) | Orijin sol-alt köşe, birim boxel, y yukarı doğru. `inject*`, `screenToGeo`, `geoToScreen` ve `queryAtScreen` bu koordinatları kullanır. Doku pikseli, boxel ile UI ölçeğinin çarpımıdır. |
| Ekran (eski stiller) | X-Plane global ekran koordinatı (boxel). `Geometry` sol-üst köşe ile genişlik ve yüksekliği tutar. |
| Konum | Derece; kuzey ve doğu pozitif |
| Yön, hız, irtifa | Gerçek yön (derece), knot, feet MSL |
| Renk | `Rgb`, 0–255 tamsayı |

## Zoom ölçeği

Harita Web Mercator projeksiyonunu kullanır. Zoom sürekli bir sayıdır; her +1 adım görüntüyü iki kat yaklaştırır. Varsayılan sınırlar 3 ile 16,5 arasıdır, mutlak üst sınır 18'dir.

| Zoom | Yaklaşık görünen genişlik (1000 boxel, 45° enlem) | Tipik kullanım |
|---|---|---|
| 3–5 | 8000–2100 NM | Kıta, ülke |
| 6–8 | 1000–260 NM | Bölge, uzun hat planlaması |
| 9–10 | 130–65 NM | Terminal sahası, yaklaşma bölgesi |
| 11–12 | 33–16 NM | Havalimanı çevresi |
| 13–14 | 8–4 NM | Havalimanı, pistler |
| 15–16,5 | 2–0,7 NM | Apron, taksi yolları |

Herhangi bir anda bir boxel'in kaç metreye karşılık geldiği `getMetersPerBoxel()` ile okunur.

## Dosyalar ve klasörler

Bir yöneticinin bütün dosyaları veri klasöründe durur. Veri klasörü `setDataDirectory()` ile verilir; verilmezse `%LOCALAPPDATA%\prime\prime-xmap\` kullanılır.

```text
<veri klasörü>/
    prime_map.json              yöneticinin merkezi ayarları
    maps/
        main/                   "main" adlı harita
            map_settings.json   görünüm, mod, katmanlar, zoom aralıkları, renkler, kaynaklar
            terrain_colors.json zemin ayarları
            map_drawings.txt    kullanıcı çizimleri
        map_2/                  ad verilmemiş harita
    cache/                      önbellek; setCacheDirectory ile başka bir yere alınabilir
        terrain/
        airports/
```

| Konu | Kural |
|---|---|
| Harita adı | `XMapConfig::mName` ya da `setName` ile verilir ve ayar klasörünün adı olur. Ad verilmezse `map_<kimlik>` kullanılır; kimlik oluşturulma sırasına bağlı olduğu için kalıcı ayar isteyen host her haritaya ad vermelidir. Aynı yöneticide iki harita aynı adı alamaz. |
| Okuma | Ayar dosyaları `XMap::initialize()` sırasında okunur. |
| Yazma | İlk açılışta, bir ayar değiştiğinde (en sık iki saniyede bir), kapanışta ve `saveSettings()` çağrıldığında. |
| Elle düzenleme | Dosyalar harita kapalıyken düzenlenebilir. Açık bir haritanın dosyası bir sonraki otomatik kayıtta yeniden yazılır. |
| Önbellek | `cache` klasörü silinebilir; içerik yeniden üretilir. |

Klasör yolları `initialize()` çağrılmadan önce de sorgulanabilir: `getDataDirectory()`, `getXMapsDirectory()`, `getSettingsFilePath()`, `getCacheDirectory()`.

## Kod ve dosya: kalıcılık

Bir haritanın ayarları hem koddan hem de ayar dosyasından gelebilir. Hangisinin geçerli olacağını `Persistence` belirler.

| Değer | `initialize()` sırasında | Değişiklikte | Ne zaman kullanılır |
|---|---|---|---|
| `LoadAndSave` (varsayılan) | Dosya okunur; dosyadaki değer koddan önce verilen değerin yerine geçer. | Dosyaya yazılır. | Kullanıcının bıraktığı durum bir sonraki açılışta geri gelsin. Koddaki değerler yalnızca ilk açılışın varsayılanı olur. |
| `SaveOnly` | Dosya okunmaz; koddaki değer her açılışta geçerlidir. | Dosyaya yazılır. | Host her şeyi kendisi belirler, dosya yalnızca bilgi içindir. |
| `Off` | Dosya okunmaz. | Yazılmaz. | Host kendi profil sistemini kullanır (`exportSettings` / `importSettings`). |

`initialize()` çağrısından sonra yapılan her ayar çağrısı uygulanır. Bir ayarın her açılışta koddaki değeri alması isteniyorsa çağrı `initialize()`'dan sonra yapılmalı ya da `SaveOnly` kullanılmalıdır. Örnek projedeki `customizeMap` fonksiyonu bu sırayı izler.

# Doku modu ayrıntıları

| Konu | Kural |
|---|---|
| Çağrı yeri | `renderTexture()` yalnızca host'un bir XPLM draw callback'i içinde çağrılır; o anda GL bağlamı etkindir. |
| Yeniden çizim | İlk çağrıda, doku boyutu değiştiğinde, mod, katman, renk ya da çizim değiştiğinde, görünüm değiştiğinde ve girdi geldiğinde hemen yeniden çizilir. Bunların dışında en fazla `maxRenderFps` kez (varsayılan 15) yeniden çizilir; böylece uçak ve uçuş izi hareket eder. Gerek yoksa çağrı hiçbir iş yapmaz. |
| Doku | RGBA, opak. `(0,0)` sol-alt, `(1,1)` sağ-üst köşedir. Boyutu `setTextureSize` değerinin ölçekle çarpımıdır. |
| Ölçek | `setTextureSize`'ın üçüncü parametresi 0 ise host'un o anki UI ölçeği kullanılır ve görüntü keskin olur. Dokuyu bir ekrana bire bir basmak için sabit bir değer verilebilir. |
| GL durumu | Kütüphane kendi kullandığı program, vertex array, tampon, framebuffer, viewport, scissor ve temizleme rengini geri koyar. Host kendi çizimine geçmeden önce `XPLMSetGraphicsState` çağırmalıdır. |
| Fare | Koordinatlar harita alanına göredir. Basma haritada başladıysa sürükleme ve bırakma, imleç alanın dışına çıksa bile iletilmelidir. `injectMouseMove`, imleç haritanın üzerindeyken her karede çağrılmalıdır; 0,25 saniye gelmezse imleç dışarıda kabul edilir. |
| Klavye | Haritanın havalimanı arama kutusu açıldığında `wantsKeyboard()` `true` olur. Host bu durumda klavye odağını alır, tuşları `injectKey` ile iletir ve `wantsKeyboard()` `false` olduğunda odağı bırakır. Odak her karede alınmamalıdır. |
| Yazı | Varsayılan yazı tipi atlası kullanılmalıdır. `setXPlaneFont(true)` ile etiketler dokuda yanlış yere düşebilir. |
| Birden çok harita | Her harita kendi dokusunu tutar; bir host birden çok haritayı farklı yerlere basabilir. |
| Pop-out | Host penceresi ayrı bir işletim sistemi penceresine alındığında GL bağlamı değişir ve harita çizilmez. |

# API başvurusu

## Fabrika ve sürüm

| Fonksiyon | Açıklama |
|---|---|
| `static XMapManager* XMapManagerFactory::createXMapManager()` | Yeni bir yönetici oluşturur. Host'un derlendiği başlık sürümü yüklü DLL'in sürümünden farklıysa `Log.txt`'ye tek satır uyarı yazar. |
| `static void XMapManagerFactory::destroyXMapManager(XMapManager*)` | Yöneticiyi ve kalan haritalarını siler; ayarlar yazılır. |
| `std::string prime::versionString()` | Yüklü kütüphanenin sürümü, örneğin `"1.0.0"`. |
| `std::string prime::libraryPath()` | Yüklü `prime_map.dll` dosyasının tam yolu. |
| `bool prime::isLibraryVersionMatching()` | Host'un derlendiği sürüm ile yüklü DLL'in sürümü aynı mı. |
| `kVersionMajor`, `kVersionMinor`, `kVersionPatch` | Başlıktaki sürüm sabitleri. |

## XMapManager

### Başlatmadan önce

| Fonksiyon | Açıklama |
|---|---|
| `void setDataDirectory(const std::string&)` | Veri klasörü. Boş bırakılırsa `%LOCALAPPDATA%/prime/prime-xmap/`. |
| `void setCacheDirectory(const std::string&)` | Önbellek klasörü. Boş bırakılırsa `<veri>/cache/`. |
| `void setLogPrefix(const std::string&)` | `Log.txt` satırlarının öneki. |
| `void setLogCallback(std::function<void(std::string_view)>)` | Log satırlarını ayrıca host'a iletir. |
| `void setTerrainCacheMb(int)` | `prime_map.json` henüz yoksa zemin disk önbelleğinin ilk sınırı (varsayılan 1024 MB, 0 sınırsız). |
| `void setPrefetchTiles(int)` | `prime_map.json` henüz yoksa önceden hazırlanacak karo sayısı (varsayılan 20, 0 kapalı). |

### Çalışma

| Fonksiyon | Açıklama |
|---|---|
| `bool initialize()` | Ortak veriyi ve arka plan işlerini başlatır. Zaten çalışıyorsa `true` döner. |
| `void shutdown()` | Açık haritaları kapatır, dosyaları yazar, arka plan işlerini durdurur. |
| `bool isInitialized() const` | Yönetici çalışıyor mu. |
| `Status getStatus() const` | Nav, havalimanı, hava yolu ve zemin dizininin hazır olup olmadığı; bekleyen zemin dokusu sayısı; ön hazırlık ilerlemesi; açık harita sayısı; geçerli önbellek sınırları; durum metni. |
| `XMapManagerSettings getSettings() const` | Merkezi ayarlar. |
| `bool setSettings(const XMapManagerSettings&)` | Merkezi ayarları hemen uygular ve `prime_map.json`'a yazar. |

### Haritalar

| Fonksiyon | Açıklama |
|---|---|
| `XMap* addXMap(const XMapConfig& = {})` | Yeni bir harita nesnesi oluşturur; harita açılmaz. Yönetici başlatılmadıysa `nullptr`. |
| `void removeXMap(XMap*)` | Ayarları yazar ve haritayı siler. |
| `XMap* findXMap(int id)`, `XMap* findXMap(const std::string& name)` | Kimliğe ya da ada göre arar; yoksa `nullptr`. |
| `std::vector<XMap*> getXMaps() const` | Bütün haritalar. |
| `int getOpenXMapCount() const` | Açık harita sayısı. |

### Yollar

| Fonksiyon | Açıklama |
|---|---|
| `getDataDirectory()`, `getSettingsFilePath()`, `getXMapsDirectory()`, `getCacheDirectory()` | Klasör ve dosya yolları. `initialize()` öncesinde de doğru değeri verir. |

### Katman bilgisi

| Fonksiyon | Açıklama |
|---|---|
| `parentOf`, `childrenOf`, `topLevelLayers`, `kindOf` | Katman ağacı. Host kendi katman menüsünü bunlarla kurabilir. |
| `layerName`, `layerKey`, `layerFromKey` | Panel adı ve ayar dosyasındaki anahtar. |
| `isLayerDefaultVisible(XMapLayer, XMapMode)` | Bir katmanın verilen modda varsayılan olarak açık olup olmadığı. |

## XMap

### Yaşam ve kimlik

| Fonksiyon | Açıklama |
|---|---|
| `bool initialize()` | Haritayı açar. `LoadAndSave` kullanılıyorsa ayar dosyalarını okur. |
| `void close()`, `bool isInitialized() const` | Haritayı kapatır; nesne ve ayarları korunur. |
| `bool isDataLoaded() const` | Ortak veri hazır ve bu haritada bekleyen zemin dokusu yok. |
| `int getId() const` | Yönetici içindeki kimlik (1, 2, ...). |
| `bool setName(const std::string&)`, `std::string getName() const` | Harita adı ve ayar klasörü. Yalnızca kapalıyken; kullanılan bir ad reddedilir. |
| `std::string getSettingsDirectory() const` | Haritanın ayar klasörü. |
| `void setPersistence(Persistence)`, `Persistence getPersistence() const` | Kalıcılık kuralı. |
| `bool saveSettings()` | Ayar dosyalarını hemen yazar. |
| `void onRender()` | Bir kare çizer. Normal kullanımda gerekmez. |

### Doku ve girdi

| Fonksiyon | Açıklama |
|---|---|
| `bool setTextureSize(int w, int h, double pixelsPerUnit = 0)` | Harita alanının boyutu (boxel). Boyut değişmediyse iş yapmaz. |
| `int getTextureWidth() const`, `int getTextureHeight() const` | Harita alanı (boxel). |
| `bool renderTexture()` | Gerekiyorsa dokuyu yeniden çizer. `false` döndüyse doku basılmamalıdır. |
| `unsigned int getTextureId() const` | `GL_TEXTURE_2D` kimliği; doku yoksa 0. |
| `bool injectMouse(int x, int y, MouseAction, MouseButton = Left)` | Basma, sürükleme, bırakma. Harita olayı kullandıysa `true`. |
| `void injectMouseMove(int x, int y)` | İmleç konumu; tooltip ve vurgu için. |
| `bool injectWheel(int x, int y, int clicks)` | İmlecin bulunduğu nokta etrafında zoom. |
| `bool injectKey(char key, int flags, char virtualKey)` | XPLM key callback değerleri olduğu gibi iletilir. |
| `bool wantsKeyboard() const` | Harita klavye girdisi bekliyor mu. |

### Pencere ve arayüz

| Fonksiyon | Açıklama |
|---|---|
| `void setStyle(WindowStyle)`, `WindowStyle getStyle() const` | Görüntüleme yolu; yalnızca kapalıyken değişir. |
| `void setTitle(const std::string&)`, `std::string getTitle() const` | Pencere başlığı (eski stiller). |
| `void setGeometry(const Geometry&)`, `Geometry getGeometry() const` | Pencere konumu ve boyutu (eski stiller). Texture'da `getGeometry` harita alanını verir. |
| `void setVisible(bool)`, `bool isVisible() const` | Gizli bir Texture haritası için `renderTexture()` `false` döner. |
| `void bringToFront()`, `void setPoppedOut(bool)`, `bool isPoppedOut() const` | Yalnızca eski stiller. |
| `void setIsUiVisible(bool)`, `bool isUiVisible() const` | Haritanın kendi arayüzü: üst şerit, katman ve mod panelleri, çizim paleti, havalimanı arama, durum şeridi. |
| `void setIsMouseInputEnabled(bool)`, `bool isMouseInputEnabled() const` | Fare ve tekerlek girdisi. Kapalıyken `inject*` çağrıları `false` döner. |

### Görünüm

| Fonksiyon | Açıklama |
|---|---|
| `void setCenter(double lat, double lon)`, `LatLon getCenter() const` | Harita merkezi. Varsayılan 39,0 / 35,0. |
| `void setZoom(double)`, `double getZoom() const` | Zoom. Varsayılan 6. |
| `static double minZoom()`, `maxZoom()`, `hardMaxZoom()` | 3; 16,5; 18. |
| `bool setZoomLimits(double min, double max)`, `getMinZoomLimit()`, `getMaxZoomLimit()` | Bu haritanın zoom sınırları. |
| `void zoomIn()`, `void zoomOut()`, `setZoomStep(double)`, `getZoomStep()` | Adımlı zoom. Varsayılan adım 0,5. |
| `panLeft()`, `panRight()`, `panUp()`, `panDown()`, `setPanStep(double)`, `getPanStep()` | Adımlı kaydırma. Adım görünen alanın oranıdır; varsayılan 0,25. |
| `void panBy(double dx, double dy)` | Boxel cinsinden kaydırma. |
| `void fitBounds(const GeoBounds&)`, `GeoBounds getVisibleBounds() const` | Bir alanı sığdırmak; görünen alanı okumak. |
| `double getMetersPerBoxel() const` | Haritanın o anki ölçeği. |
| `void setFollowAircraft(bool)`, `bool isFollowingAircraft() const`, `void centerOnAircraft()` | Uçak takibi. Kullanıcı haritayı elle kaydırınca takip kapanır. |
| `bool goToAirport(const std::string&)` | ICAO kodu, kimlik ya da ada göre havalimanını ekrana sığdırır. |
| `std::vector<AirportMatch> findAirports(const std::string& query, int maxResults) const` | Havalimanı arama. |
| `LatLon screenToGeo(int x, int y) const`, `bool geoToScreen(double lat, double lon, int* x, int* y) const` | Harita alanı ile coğrafi koordinat arasında dönüşüm. |

### Mod

| Fonksiyon | Açıklama |
|---|---|
| `void setMode(XMapMode)`, `XMapMode getMode() const`, `void nextMode()` | Scenery, Vfr, IfrLow, IfrHigh, Dark. Katman durumları her mod için ayrı saklanır. |

### Katmanlar

| Fonksiyon | Açıklama |
|---|---|
| `void setIsLayerVisible(XMapLayer, bool)`, `bool isLayerVisible(XMapLayer) const`, `void toggleLayer(XMapLayer)` | Geçerli moddaki görünürlük. |
| `void setLayersVisible(std::initializer_list<XMapLayer>, bool)` | Birden çok katmanı birlikte açar ya da kapatır. |
| `void showOnlyLayers(std::initializer_list<XMapLayer>)` | Yalnızca verilen katmanları açık bırakır. |
| `void resetLayersToDefaults()` | Geçerli modun varsayılanlarına döner. |
| `LayerState getLayerState(XMapLayer) const` | `Off`, `On` ya da alt katmanların bir kısmı açıksa `Mixed`. |
| `bool setLayerZoomRange(XMapLayer, double min, double max)` | Katmanın görüneceği zoom aralığı. Ayrıntı için "Zoom aralığı" bölümüne bakın. |
| `void getLayerZoomRange(XMapLayer, double* min, double* max) const` | Geçerli aralık. |
| `void clearLayerZoomRange(XMapLayer)`, `void clearAllLayerZoomRanges()` | Yerleşik aralığa dönüş. |
| `void getDefaultLayerZoomRange(XMapLayer, double* min, double* max) const` | Yerleşik aralık. |
| `double getLayerHardMinZoom(XMapLayer) const` | Aralığın aşağı çekilebileceği en küçük zoom. |
| `bool setColor(XMapLayer, const Rgb&)`, `Rgb getColor(XMapLayer) const` | Katman rengi. Renk verilemeyen katmanda `false`; atanmış renk yoksa `getColor` sonucunda `mR < 0`. |
| `void clearColor(XMapLayer)`, `void clearAllColors()` | Modun kendi rengine dönüş. |

### Çizimler

| Fonksiyon | Açıklama |
|---|---|
| `void setDrawTool(DrawTool)`, `DrawTool getDrawTool() const` | Kullanıcının fareyle kullanacağı araç: Pan, Point, Route, Region. |
| `bool addDrawingPoint(double lat, double lon)`, `bool finishDrawing()` | Koddan çizim. Seçili araca göre nokta, rota ya da bölge oluşur. |
| `bool undoDrawing()`, `void clearDrawings()` | Son adımı geri alır; bütün çizimleri siler. |
| `bool hasUnfinishedDrawing() const` | Bitmemiş bir rota ya da bölge var mı. |
| `std::vector<Shape> getShapes() const`, `bool removeShape(int id)` | Bitmiş şekiller ve tek şekil silme. |
| `std::string exportDrawings() const`, `bool importDrawings(const std::string&)` | Çizimleri `map_drawings.txt` biçiminde dışa ve içe aktarma. |

### Uçak ve rota

| Fonksiyon | Açıklama |
|---|---|
| `void setAircraftSource(DataSource)`, `DataSource getAircraftSource() const` | `XPlane`: dataref'ler. `Host`: `setAircraft` ile verilen değer. |
| `void setAircraft(const AircraftState&)`, `AircraftState getAircraft() const` | Host kaynağında uçak durumu; her güncellemede çağrılır. |
| `void clearTrack()` | Uçuş izini siler. |
| `void setRouteSource(DataSource)`, `DataSource getRouteSource() const` | `XPlane`: X-Plane FMS'i. `Host`: `setRoute` ile verilen rota. |
| `void setRoute(const std::vector<RouteWaypoint>&, int activeIndex)` | Host rotası ve etkin bacak. |

### Sorgu

| Fonksiyon | Açıklama |
|---|---|
| `std::vector<XMapObject> queryAt(double lat, double lon) const` | O noktadaki nesneler, en üstteki önce. |
| `std::vector<XMapObject> queryAtScreen(int x, int y) const` | Aynı sorgu, harita alanı koordinatıyla. |

### Çizim seçenekleri

| Fonksiyon | Açıklama |
|---|---|
| `void setMaxRenderFps(int)`, `int getMaxRenderFps() const` | Değişiklik yokken saniyedeki en fazla yeniden çizim (varsayılan 15). |
| `void setImageCache(bool)`, `bool isImageCacheEnabled() const` | Eski stillerde değişmeyen kareyi tekrar çizmeme. |
| `void setXPlaneFont(bool)`, `bool isXPlaneFont() const` | X-Plane yazı tipini kullanma. Texture'da önerilmez. |

### Ayarlar

| Fonksiyon | Açıklama |
|---|---|
| `std::string exportSettings() const`, `bool importSettings(const std::string&)` | `map_settings.json` içeriğini metin olarak dışa ve içe aktarma. Pencere ve görünüm bölümleri de aktarılır. |
| `TerrainSettings getTerrainSettings() const`, `void setTerrainSettings(const TerrainSettings&)` | Bu haritanın zemin ayarları; hemen uygulanır. |
| `std::string exportTerrainSettings() const`, `bool importTerrainSettings(const std::string&)` | Zemin ayarlarının metin olarak aktarımı. |
| `uint64_t getSettingsRevision() const`, `uint64_t getDrawingsRevision() const` | Ayarlar ya da çizimler değiştikçe artan sayaçlar. |

### Durum ve ön hazırlık

| Fonksiyon | Açıklama |
|---|---|
| `std::string getStatusText() const`, `int getTerrainPendingImages() const` | Haritanın durum metni ve bekleyen zemin dokusu sayısı. |
| `bool precacheVisibleArea()`, `bool precacheArea(const GeoBounds&)` | Bir alanın kurulu sahnesini diskte önceden hazırlar. Önbellek sınırının %80'ini aşacaksa başlamaz. |
| `void cancelPrecache()` | Ön hazırlığı durdurur. |
| `void setDiagnostics(bool)`, `bool isDiagnosticsOn() const` | `Log.txt`'ye periyodik ölçüm ve durum şeridinde çizim süresi. |

## Tipler

| Tip | Alanlar ve değerler |
|---|---|
| `XMapConfig` | `mName`, `mPersistence` (LoadAndSave), `mGeometry`, `mStyle` (Texture), `mMode` (Dark), `mUiVisible` (false), `mMouseInput` (false), `mVisible` (true), `mImageCache` (true), `mMaxRenderFps` (15), `mXPlaneFont` (false) |
| `XMapMode` | `Scenery`, `Vfr`, `IfrLow`, `IfrHigh`, `Dark` |
| `WindowStyle` | `Texture`; eski stiller `Embedded`, `Standalone` |
| `Persistence` | `Off`, `SaveOnly`, `LoadAndSave` |
| `MouseAction`, `MouseButton` | `Down`, `Drag`, `Up`; `Left`, `Right` |
| `DrawTool` | `Pan`, `Point`, `Route`, `Region` |
| `DataSource` | `XPlane`, `Host` |
| `LayerKind`, `LayerState` | `Group`, `Layer`, `SubLayer`; `Off`, `On`, `Mixed` |
| `LatLon` | `mLat`, `mLon` |
| `GeoBounds` | `mSouth`, `mWest`, `mNorth`, `mEast` |
| `Geometry` | `mLeft`, `mTop` (üst kenarın y değeri), `mWidth`, `mHeight` |
| `Rgb` | `mR`, `mG`, `mB` (0–255) |
| `Status` | `mNavReady`, `mAirportsReady`, `mAirwaysReady`, `mTerrainIndexReady`, `mTerrainPendingImages`, `mPrecacheRunning`, `mPrecacheDone`, `mPrecacheTotal`, `mText`, `mPerformanceText`, `mOpenMaps`, `mTerrainCacheLimitMb`, `mTextureLimitMb` |
| `AirportMatch` | `mIdent`, `mName`, `mPosition` |
| `XMapObject` | `mLayer`, `mText` |
| `Shape` | `mId`, `mKind` (`Point`, `Route`, `Region`), `mLabel`, `mPoints` |
| `AircraftState` | `mValid`, `mPosition`, `mHeadingDeg`, `mGroundSpeedKt`, `mAltitudeFt` |
| `RouteWaypoint` | `mIdent`, `mPosition`, `mAltitudeFt` |
| `TerrainSettings` | "Zemin ayarları" bölümü |
| `XMapManagerSettings` | "prime_map.json" bölümü |

# Katmanlar

## Katman ağacı ve görünürlük

Katmanlar beş grup altında toplanır: XMap (zemin, yollar, ızgara), Airports, Navaids, Airways ve Flight. Kullanıcı çizimleri gruba bağlı değildir. Bazı katmanların alt katmanları vardır; örneğin `Runways` altında kaplamalı, kaplamasız ve su pistleri, uzantılar, işaretler ve numaralar bulunur.

- Bir üst öğeyi kapatmak altındakileri gizler; alt öğelerin kendi durumu korunur.
- Kapalı bir katmanın tek bir alt katmanını açmak yalnızca o alt katmanı açar; katmanın durumu `Mixed` olur.
- Son açık alt katman kapatılınca katman da kapanır. Bütün alt katmanları kapalı bir katman açılınca hepsi açılır.
- Bir grubu açmak, o grubun geçerli modda varsayılan olarak açık olan katmanlarını açar.
- Görünürlük her mod ve her harita için ayrı saklanır. Mod değiştiğinde o modun son durumu gelir.

## Zoom aralığı

Her katmanın yerleşik bir görünme aralığı vardır; örneğin pistler zoom 11'den, fix'ler zoom 9'dan itibaren çizilir. Host `setLayerZoomRange` ile bu aralığı değiştirebilir.

- Host'un verdiği aralık yerleşik aralığın yerine geçer. Katman daha erken de, daha geç de gösterilebilir.
- Aşağı çekme, katmanın sert alt sınırına kadar serbesttir. Sert alt sınırın altındaki istek o sınıra çekilir ve `Log.txt`'ye `min ... clamped to ...` uyarısı yazılır. Sert alt sınırlar, katmanın en yoğun bölgede çizim maliyeti ölçülerek belirlenmiştir.
- `getLayerZoomRange` geçerli aralığı, `getDefaultLayerZoomRange` yerleşik aralığı, `getLayerHardMinZoom` sert alt sınırı verir.
- `clearLayerZoomRange` ya da `(0, 99)` aralığı yerleşik değere döndürür.
- Bir grup verilirse aralık grubun bütün katmanlarına uygulanır. Alt katmanlara ayrı ayrı aralık verilebilir.
- Etiket, yön oku, VOR gülü gibi ayrıntılar katmanın içinde kendi zoom eşikleriyle çizilir. Katman daha erken gösterildiğinde bu ayrıntılar kendi eşiklerine gelene kadar çizilmez.
- Havalimanı sembollerinde kategoriye göre yoğunluk vardır: büyük havalimanları 4, küçükler 8, deniz üsleri 9, heliport ve kapalı havalimanları 11'den itibaren görünür. Aralık aşağı çekildiğinde bütün kategoriler aynı miktarda erkene kayar. Aralık yukarı çekildiğinde verilen değerden önce hiçbir havalimanı çizilmez; kategoriler kendi eşiklerini korur.
- Tabloda eşiği olmayan katmanlar (ızgara, uçak, rota, iz, çizimler) her zoomda çizilir.

```cpp
map->setLayerZoomRange(prime::XMapLayer::AirwaysGroup, 8.0, 99.0);
map->setLayerZoomRange(prime::XMapLayer::Fixes, 7.0, 99.0);
map->setLayerZoomRange(prime::XMapLayer::Terrain, 0.0, 12.0);
map->setLayerZoomRange(prime::XMapLayer::RoadsLocal, 13.0, 99.0);
map->clearLayerZoomRange(prime::XMapLayer::Fixes);
```

Birinci satır hava yollarını zoom 8'den itibaren gösterir. İkinci satır fix'leri yerleşik 9 yerine 7'den itibaren gösterir. Üçüncü satır zemini zoom 12'den yakında gizler. Dördüncü satır yalnızca yerel yolların eşiğini değiştirir. Son satır fix'leri yerleşik aralığa döndürür.

## Katman tablosu

Sütunlar: ayar dosyasındaki anahtar, haritanın katman panelindeki ad, yerleşik görünme eşiği, sert alt sınır, beş modda varsayılan görünürlük ve renk verilip verilemeyeceği. Mod sütunları sırasıyla Scenery, VFR, IFR Low, IFR High ve Dark'tır; ● açık, ○ kapalı anlamındadır. Eşik sütunu boş olan alt katman, üst katmanının eşiğini kullanır.

<!-- LAYER_TABLE -->

## Renk verilebilen katmanlar

| Katman | Değişen öğe |
|---|---|
| `Grid` | Enlem ve boylam çizgileri |
| `RoadsMotorways`, `RoadsPrimary`, `RoadsSecondary`, `RoadsLocal`, `RoadsRailways`, `RoadsPowerLines` | İlgili yol, demiryolu ve enerji hattı çizgileri |
| `Vor`, `Ndb`, `Ils`, `Fixes` | Sembol, etiket ve ILS yaklaşma şekli |
| `AirwaysLow`, `AirwaysHigh` | Hava yolu çizgileri |
| `Runways` | Pist kenar çizgisi |
| `TerrainBuiltUp` | Zemin dokusundaki yerleşim alanı rengi |

Renkler bütün modlarda geçerlidir. Diğer katmanların rengi modun temasından gelir.

```cpp
map->setColor(prime::XMapLayer::AirwaysLow, prime::Rgb{255, 140, 0});
prime::Rgb tColor = map->getColor(prime::XMapLayer::Vor);
map->clearColor(prime::XMapLayer::AirwaysLow);
```

# Zemin ayarları

Her haritanın zemin görünümü `TerrainSettings` ile ayarlanır ve `terrain_colors.json` dosyasında saklanır.

```cpp
prime::TerrainSettings tTerrain = map->getTerrainSettings();
tTerrain.mUseXPlaneColors = false;
tTerrain.mClassColors = {{"water", {20, 60, 120}}, {"forest_conifer", {40, 80, 50}}};
tTerrain.mShadingStrength = 0.9;
tTerrain.mVfrContourLines = true;
map->setTerrainSettings(tTerrain);
```

Arazi sınıfları: `water`, `urban`, `forest_conifer`, `forest_broadleaf`, `shrub`, `grass`, `crop`, `wetland`, `rock`, `snow_ice`, `tundra`, `sand`, `airport`, `land`.

| Dosya bölümü ve anahtar (varsayılan) | `TerrainSettings` alanı | Etki |
|---|---|---|
| `palette.source` (`xplane`) | `mUseXPlaneColors` | `xplane`: sınıf renkleri X-Plane'in arazi dokularından hesaplanır. `colors`: `colors` bölümündeki değerler kullanılır. |
| `colors.<sınıf>` (`"R G B"`) | `mClassColors` | Arazi sınıfı renkleri. Su rengi her zaman buradan alınır. |
| `shading.strength` (0,45) | `mShadingStrength` | Kabartma gölgesinin gücü, 0–1. |
| `blend.meters` (150), `blend.max_pixels` (5) | `mBlendMeters`, `mBlendMaxPixels` | Scenery modunda sınıflar arası yumuşak geçiş; 0 kapalı. |
| `warp.meters` (65), `warp.wavelength` (250) | `mWarpMeters`, `mWarpWavelength` | Sınıf sınırlarının doğal kıvrımı; 0 düz. |
| `closeup.class_mix` (0,3) | `mCloseupClassMix` | Yakın zoomda sınıf renklerinin ortak kara tonuna yaklaşması. |
| `relief.multi_directional` (1) | `mReliefMultiDirectional` | Kabartmada dört yönlü ışık. |
| `relief.exaggeration` (2,5), `relief.coarse_boost` (0,35) | `mReliefExaggeration`, `mReliefCoarseBoost` | Eğim abartması ve uzak zoomda ek abartma. |
| `relief.slope_tone` (0,12), `relief.flat_slope` (0,02) | `mReliefSlopeTone`, `mReliefFlatSlope` | Dik yamaçların koyulaşması ve düz ova eşiği. |
| `water.coastline` (1), `water.coastline_strength` (0,5) | `mCoastline`, `mCoastlineStrength` | Kıyı çizgisi. |
| `water.band_meters` (200), `band_max_pixels` (24), `band_strength` (0,1) | `mWaterBandMeters`, `mWaterBandMaxPixels`, `mWaterBandStrength` | Kıyıdan açığa doğru açık renkli şerit. |
| `water.depth_tone` (0,25), `water.depth_full_meters` (200) | `mDepthTone`, `mDepthFullMeters` | Derinliğe göre koyulaşma. |
| `vfr.bands` (1), `vfr.contour_lines` (0), `vfr.contour_strength` (0,35) | `mVfrBands`, `mVfrContourLines`, `mVfrContourStrength` | VFR modunda yükseklik bantları ve bant sınırında kontur çizgisi. |
| `vfr.band_1` … `vfr.band_8` (`"<ft> R G B"`) | `mVfrBandsFeet` | Bantların alt sınırı (feet, artan sırada) ve renkleri. |

# Ayar dosyaları

Bütün ayar dosyaları JSON biçimindedir: `{ "bölüm": { "anahtar": "değer" } }`. Değerler tırnak içinde metindir, ondalık ayırıcı noktadır, mantıksal değerler `"1"` ve `"0"`'dır. Dosyada bulunmayan anahtar koddan verilen ya da varsayılan değeri alır. Bir dosyayı silmek varsayılanları geri getirir.

## map_settings.json

| Bölüm | Anahtarlar | API karşılığı |
|---|---|---|
| `window` | `left`, `top`, `right`, `bottom`, `visible` | `setGeometry`, `setVisible` (yalnızca Standalone'da geri yüklenir) |
| `view` | `mode` (`scenery`, `vfr`, `ifr_low`, `ifr_high`, `dark`), `center_lat`, `center_lon`, `zoom`, `follow_aircraft` | `setMode`, `setCenter`, `setZoom`, `setFollowAircraft` |
| `view` | `show_controls`, `mouse_input`, `min_zoom`, `max_zoom`, `zoom_step`, `pan_step` | `setIsUiVisible`, `setIsMouseInputEnabled`, `setZoomLimits`, `setZoomStep`, `setPanStep` |
| `render` | `cache`, `max_fps` | `setImageCache`, `setMaxRenderFps` |
| `text` | `renderer` (`atlas`, `xplane`) | `setXPlaneFont` |
| `tools` | `visible`, `collapsed`, `offset_right`, `offset_top` | Çizim paletinin durumu |
| `layers.<mod>` | `<katman anahtarı>`: `"1"` ya da `"0"` | `setIsLayerVisible` |
| `layer_zoom` | `<katman anahtarı>`: `"<min> <max>"` | `setLayerZoomRange`; yalnızca host'un verdiği aralıklar yazılır |
| `layer_colors` | `<katman anahtarı>`: `"R G B"` | `setColor`; yalnızca atanmış renkler yazılır |
| `sources` | `aircraft`, `route` (`xplane`, `host`) | `setAircraftSource`, `setRouteSource` |

```json
{
  "view": { "mode": "dark", "center_lat": "47.45", "center_lon": "-122.3", "zoom": "10",
            "follow_aircraft": "0", "min_zoom": "6", "max_zoom": "15" },
  "layer_zoom": { "fix": "7.00 99.00", "airways_low": "8.00 99.00" },
  "layer_colors": { "airways_low": "255 140 0" },
  "sources": { "aircraft": "xplane", "route": "host" }
}
```

## terrain_colors.json

Bölümleri ve anahtarları "Zemin ayarları" tablosunda verilmiştir. `palette.version` anahtarı kütüphane tarafından yönetilir ve elle değiştirilmemelidir.

## map_drawings.txt

Her satır bitmiş bir şekildir: `<tür> <numara> <enlem> <boylam> ...`. Tür `P` nokta, `R` rota (en az iki köşe), `A` bölge (en az üç köşe) demektir. `#` ile başlayan satırlar okunmaz. `Shape::mId` değeri tür kodu × 1 000 000 + numaradır; tür kodu nokta için 0, rota için 1, bölge için 2'dir.

```text
P 1 47.4502 -122.3088
R 1 47.45 -122.31 47.53 -122.30 47.61 -122.33
A 1 47.40 -122.40 47.40 -122.20 47.50 -122.20 47.50 -122.40
```

## prime_map.json

Önbellek ve doku belleği bir yöneticinin haritaları arasında ortaktır. Sınırlar açık harita sayısı (n) arttıkça büyür: `sınır(n) = taban × (1 + artış% × (n − 1) / 100)`; tavan verilmişse onu aşmaz.

| Bölüm ve anahtar | `XMapManagerSettings` alanı | Varsayılan | Anlamı |
|---|---|---|---|
| `cache.terrain_mb` | `mTerrainCacheMb` | 1024 | Zemin disk önbelleği (MB, tek harita için; 0 sınırsız) |
| `cache.growth_percent_per_map` | `mCacheGrowthPercentPerXMap` | 25 | Her ek açık harita için artış |
| `cache.max_terrain_mb` | `mTerrainCacheMaxMb` | 0 | Tavan (0 tavan yok) |
| `cache.prefetch_tiles` | `mPrefetchTiles` | 20 | Uçağın çevresi ve rotası için önceden hazırlanan karo sayısı (0 kapalı) |
| `memory.texture_mb` | `mTextureMemoryMb` | 512 | Zemin dokuları için video belleği (MB, tek harita için) |
| `memory.growth_percent_per_map` | `mMemoryGrowthPercentPerXMap` | 50 | Her ek açık harita için artış |
| `memory.max_texture_mb` | `mTextureMemoryMaxMb` | 2048 | Tavan |
| `maps.autosave` | `mAutoSave` | 1 | Değişiklikte otomatik kayıt |
| `maps.autosave_seconds` | `mAutoSaveSeconds` | 2 | Aynı haritanın iki kaydı arasındaki en kısa süre |

```cpp
prime::XMapManagerSettings tSettings = manager->getSettings();
tSettings.mTerrainCacheMb = 4096;
manager->setSettings(tSettings);
```

# Birden çok harita ve paylaşım

Paylaşımın birimi yöneticidir. Aynı yöneticideki haritalar nav verisini, havalimanı indeksini, hava yollarını, zemin üreticisini ve doku belleğini paylaşır; bu bellek açısından en verimli kurulumdur. İki ayrı yönetici iki ayrı veri kopyası ve iki ayrı zemin üreticisi demektir.

Her yöneticinin iki klasörü vardır. Ayarlar her zaman **veri klasöründe** durur; zemin ve havalimanı önbelleği ayrı bir **önbellek klasörüne** alınabilir. Önbellek ayarlardan bağımsızdır: renk, mod ya da zemin ayarı farklı olan haritalar aynı önbelleği kullanabilir. Önbellek dosyaları benzersiz adlı geçici dosyalar üzerinden yazılır ve yerine konur; aynı önbelleği kullanan iki yönetici birbirinin dosyasını bozmaz.

## Tek yönetici, birden çok harita

Önerilen kurulumdur. Her haritaya ayrı bir ad verilir; ayarları ayrı klasörlerde tutulur, veri ve önbellek ortaktır.

```cpp
prime::XMapManager* tManager = prime::XMapManagerFactory::createXMapManager();
tManager->setDataDirectory(myPluginFolder);
tManager->initialize();
prime::XMap* tOverview = tManager->addXMap({.mName = "overview", .mMode = prime::XMapMode::IfrHigh});
prime::XMap* tDetail = tManager->addXMap({.mName = "detail"});
tOverview->initialize();
tDetail->initialize();
```

## İki host eklentisi

Her eklenti kendi yöneticisini kurar. Veri klasörleri ayrı olmalıdır; önbellek ortak verilebilir.

```cpp
tManager->setDataDirectory("C:/X-Plane 11/Resources/plugins/eklentiA/xmap");
tManager->setCacheDirectory("C:/X-Plane 11/Resources/plugins/xmap-cache");
```

Veri klasörü verilmeyen her yönetici `%LOCALAPPDATA%\prime\prime-xmap\` klasörünü kullanır. Resmî Prime XMap eklentisi de bu klasörü kullanır. İki yönetici aynı veri klasörünü kullanırsa ortak `prime_map.json` dosyasını ve aynı adlı haritaların ayar klasörlerini birbirinin üzerine yazar; harita adı denetimi yalnızca bir yöneticinin kendi haritaları arasında yapılır. Bu durumda ikinci yönetici `initialize()` sırasında `Log.txt`'ye uyarı yazar.

## Ayarların kopyalanması

```cpp
tMapB->importSettings(tMapA->exportSettings());
tMapB->importTerrainSettings(tMapA->exportTerrainSettings());
tMapB->importDrawings(tMapA->exportDrawings());
```

`exportSettings` pencere ve görünüm bölümlerini de içerir; kopya, kaynağıyla aynı merkezde ve zoomda açılır. Farklı bir görünüm isteniyorsa aktarımdan sonra `setCenter` ve `setZoom` çağrılır.

## Eş zamanlı haritalar

Kütüphane iki haritayı kendiliğinden eş zamanlı tutmaz; ortak klasör kullanmak da bunu sağlamaz, çünkü ayarlar yalnızca `initialize()` sırasında okunur. Host bunu iki yoldan biriyle yapar: her ayar çağrısını iki haritaya birden yapar ya da revizyon sayaçlarını izler.

```cpp
if (tMapA->getSettingsRevision() != tLastSettings)
{
    tLastSettings = tMapA->getSettingsRevision();
    tMapB->importSettings(tMapA->exportSettings());
}
```

## Aynı X-Plane'de birden çok eklenti

Windows aynı adlı DLL'i bir süreçte bir kez yükler. Aynı X-Plane'de `prime_map.dll` kullanan bütün eklentiler, ilk yüklenen dosyayı paylaşır. Bu nedenle bütün eklentiler kütüphanenin aynı sürümüyle derlenmeli ve aynı `prime_map.dll` dosyasını taşımalıdır. Sürümler farklıysa `createXMapManager` `Log.txt`'ye `host built with X, loaded DLL is Y at <yol>` uyarısını yazar; host `isLibraryVersionMatching()` ile bu denetimi kendisi de yapabilir.

Log öneki de süreç geneline aittir: aynı DLL'i paylaşan yöneticilerde en son verilen `setLogPrefix` değeri bütün satırlarda görünür.

# Tarifler

## Siyah zeminde yalnızca havalimanları

```cpp
map->setMode(prime::XMapMode::Dark);
map->setIsLayerVisible(prime::XMapLayer::Terrain, false);
map->showOnlyLayers({prime::XMapLayer::AirportSymbols, prime::XMapLayer::Runways});
```

Dark mod zemin olarak kara ve suyu ayıran koyu bir görünüm kullanır ve Terrain katmanı açıkken DSF dosyalarını okur. Terrain katmanı kapatıldığında hiçbir DSF okunmaz ve arka plan düz siyah olur. Bu kurulumda zemin hiç kullanılmayacaksa `setPrefetchTiles(0)` ile ön hazırlık da kapatılabilir. Pistleri daha uzak zoomda göstermek için `setLayerZoomRange(Runways, ...)` kullanılır.

## Uçak merkezli küçük navigasyon ekranı

```cpp
map->setIsUiVisible(false);
map->setIsMouseInputEnabled(false);
map->setFollowAircraft(true);
map->setZoomLimits(6.0, 14.0);
map->setRouteSource(prime::DataSource::Host);
map->setRoute(tWaypoints, tActiveLeg);
```

Ekranın düğmeleri `zoomIn()` ve `zoomOut()` çağırır. Uçuş planı değiştiğinde `setRoute` yeniden çağrılır.

## Host'un kendi uçak verisi

```cpp
map->setAircraftSource(prime::DataSource::Host);
prime::AircraftState tState;
tState.mValid = true;
tState.mPosition = {41.27, 28.75};
tState.mHeadingDeg = 350.0;
tState.mGroundSpeedKt = 140.0;
tState.mAltitudeFt = 3000.0;
map->setAircraft(tState);
```

Bir eğitmen istasyonunun başka bir uçağı izlemesi gibi durumlarda kullanılır. `setAircraft` her güncellemede çağrılır.

## Harita üzerine host sembolü

```cpp
int tX = 0;
int tY = 0;
if (map->geoToScreen(tLat, tLon, &tX, &tY))
{
    // (mapLeft + tX, mapBottom + tY) noktasina host kendi sembolunu cizer
}
```

Host, haritanın dokusunu bastıktan sonra kendi sembollerini aynı alanın üzerine çizer.

## Tıklanan nesneyi bulmak

```cpp
const prime::LatLon tWhere = map->screenToGeo(x - mapLeft, y - mapBottom);
for (const prime::XMapObject& tObject : map->queryAtScreen(x - mapLeft, y - mapBottom))
{
    // tObject.mLayer ve tObject.mText, ornegin "KSEA Seattle Tacoma Intl"
}
```

## Kullanıcı çizimlerini dışarı almak

```cpp
for (const prime::Shape& tShape : map->getShapes())
{
    // tShape.mKind, tShape.mLabel, tShape.mPoints
}
const std::string tSaved = map->exportDrawings();
map->importDrawings(tSaved);
```

## Bir bölgeyi önceden hazırlamak

```cpp
map->precacheArea({40.0, 25.0, 43.0, 30.0});
const prime::Status tStatus = manager->getStatus();
// tStatus.mPrecacheDone / tStatus.mPrecacheTotal
```

## Seçili katmanlı, zoom sınırlı gömülü panel

```cpp
map->showOnlyLayers({prime::XMapLayer::Terrain, prime::XMapLayer::AirportSymbols,
                     prime::XMapLayer::Runways, prime::XMapLayer::Vor});
map->setZoomLimits(7.0, 13.0);
map->setLayerZoomRange(prime::XMapLayer::Runways, 9.0, 99.0);
```

# Sık sorulan sorular

### Haritayı yerleştirmek için kendi pencerem mi gerekiyor?
Evet. Texture yolunda kütüphane pencere açmaz; host kendi penceresinde ya da panelinde dokuyu basar ve girdileri iletir.

### `renderTexture()` her karede çağrılmalı mı, pahalı mı?
Her karede çağrılmalıdır. Değişiklik yoksa iş yapmaz; değişiklik olmadığında saniyede en fazla `maxRenderFps` kez yeniden çizer.

### Harita boş ya da gri görünüyor.
`initialize()` çağrıldığından ve `setTextureSize` değerlerinin sıfırdan büyük olduğundan emin olun. Veri yükleniyor olabilir: `getStatusText()` ve `isDataLoaded()` durumu gösterir. Zemin yalnızca kurulu sahnenin bulunduğu yerlerde ve zoom 5'ten yakında çizilir.

### Kodda verdiğim ayar her açılışta dosyadaki değerle değişiyor.
`LoadAndSave` kalıcılığında `initialize()` dosyayı okur ve önceden verilen değerlerin yerine koyar. Ayarı `initialize()`'dan sonra verin ya da `Persistence::SaveOnly` kullanın.

### İki haritam aynı ayarları gösteriyor.
Her haritaya farklı bir ad verin. Haritalar farklı eklentilerdeyse her eklenti `setDataDirectory` ile kendi klasörünü vermelidir.

### Bir katman istediğim kadar uzaktan görünmüyor.
`setLayerZoomRange` ile aralığı aşağı çekin. İstek sert alt sınırın altındaysa sınıra çekilir ve `Log.txt`'de `clamped` uyarısı görünür. Sınırlar katman tablosundadır.

### Dark modda neden DSF okunuyor?
Dark modun zemini kara ve suyu ayırır ve bunun için sahne verisini kullanır. Zemin istenmiyorsa Terrain katmanını kapatın.

### Uçak konumunu ve rotayı kendim verebilir miyim?
`setAircraftSource(DataSource::Host)` ve `setAircraft`; rota için `setRouteSource(DataSource::Host)` ve `setRoute`.

### Kullanıcının çizdiği rotayı kendi sistemime nasıl alırım?
`getShapes()` bütün şekilleri koordinatlarıyla verir; `exportDrawings()` metin olarak verir.

### Tıklanan yerde ne olduğunu nasıl öğrenirim?
`queryAtScreen(x, y)` ya da `queryAt(lat, lon)`.

### Renkleri değiştirebilir miyim?
15 katmanın rengi `setColor` ile, zemin renkleri `TerrainSettings` ile değiştirilir. Diğer katmanların rengi modun temasından gelir.

### Kaç yönetici açmalıyım?
Genellikle bir tane. Birden çok harita aynı yöneticide bellek ve işlemciyi en verimli kullanır. Ayrı yönetici yalnızca ömrü bağımsız ayrı eklentiler için gerekir ve bu durumda veri klasörleri ayrı olmalıdır.

### Log satırlarım başka eklentinin önekiyle görünüyor.
Log öneki süreç geneline aittir; aynı DLL'i kullanan eklentilerde en son verilen önek geçerlidir.

### Aynı X-Plane'deki iki eklenti farklı `prime_map.dll` sürümü getirirse ne olur?
İkinci eklenti de ilk yüklenen DLL'i kullanır. Sürümler farklıysa `Log.txt`'de uyarı ve yüklenen dosyanın yolu görünür. Bütün eklentiler aynı sürümle derlenmelidir.

### X-Plane 12'de çalışır mı?
Bu sürüm X-Plane 11 içindir.

### API'yi başka bir thread'den çağırabilir miyim?
Hayır. Bütün çağrılar X-Plane ana thread'inden yapılmalıdır.

### Hangi dosyaları elle düzenleyebilirim?
`map_settings.json`, `terrain_colors.json`, `map_drawings.txt` ve `prime_map.json`. Düzenleme harita kapalıyken yapılmalıdır.

### Önbellek ne kadar yer kaplar, silinebilir mi?
Zemin önbelleği varsayılan olarak 1024 MB ile sınırlıdır ve sınır aşılınca en eski karolar silinir. Havalimanı indeksi yaklaşık 9 MB'dır. `cache` klasörü silinebilir; içerik yeniden üretilir.

# Sınırlar

- Bu sürüm X-Plane 11 ve Windows x64 içindir.
- Renk 15 katmana verilebilir. Çizgi kalınlığı, sembol ve yazı boyutu ayarlanamaz. Host yeni bir tema tanımlayamaz.
- Harita kuzey yukarı çizilir; uçak yönüne göre döndürme yoktur.
- Etiket ve ok gibi katman içi ayrıntı eşikleri değiştirilemez.
- Zemin karolarının ilk üretimi zaman alır. Önceden hazırlama ve disk önbelleği bunu hafifletir.
- Texture haritası, ayrı bir işletim sistemi penceresine alınmış host penceresinde çizilmez.
- Genel başlıklar kütüphanenin iç başlıklarını da içerir. Sürümler arasında ikili uyumluluk garanti edilmez; host, kullandığı kütüphane sürümüyle yeniden derlenmelidir.
- Bina ve üç boyutlu nesneler gösterilmez. 180. boylam civarında çizim eksik olabilir. Etiketler ASCII karakterlerle yazılır. ILS yönü gerçek yöndür.

# Sürüm notları

## 1.0.0

İlk kararlı sürüm.

- Yeni haritalar varsayılan olarak Dark modda açılır. Ayar dosyası olan haritalar dosyadaki modu korur.
- Katmanların görünme eşikleri tek bir tabloda toplandı. Host aralığı yerleşik eşiğin yerine geçer; aşağı çekme ölçülmüş sert alt sınıra kadar serbesttir. VOR, NDB, ILS, fix ve havalimanı sembolleri de bu kurala dahildir. `getDefaultLayerZoomRange` ve `getLayerHardMinZoom` eklendi.
- Zemin ön hazırlığı, hiç harita açılmamışken de uçağın çevresinde çalışır.
- Önbellek dosyaları benzersiz adlı geçici dosyalar üzerinden yazılır; yarım kalan geçici dosyalar temizlenir.
- `createXMapManager` host ile DLL sürümünü karşılaştırır. Açılış satırı yüklenen DLL'in yolunu yazar. `versionString`, `libraryPath` ve `isLibraryVersionMatching` eklendi.
- Kapanıştan sonra elde kalan harita işaretçileri güvenli şekilde etkisizdir.
- `setColor(TerrainBuiltUp)` zemin görüntüsüne uygulanır.
- Aynı veri klasörünü kullanan ikinci yönetici uyarı verir.

## 0.2x sürümlerinden geçiş

| 0.23 ve öncesi | 1.0.0 |
|---|---|
| `prime::MapLibrary::Get().Init(InitConfig)` | `XMapManagerFactory::createXMapManager()`, ardından `set...` ve `initialize()` |
| `MapLibrary::CreateMap()`, `Map::Open()`, `Map::Close()` | `addXMap()`, `XMap::initialize()`, `XMap::close()`, `removeXMap()` |
| `MapMode`, `MapLayer`, `LibrarySettings`, `MapObject`, `OpenConfig` | `XMapMode`, `XMapLayer`, `XMapManagerSettings`, `XMapObject`, `XMapConfig` |
| `SetLayerVisible`, `SetUiVisible`, `SetMouseInput`, `VersionString` | `setIsLayerVisible`, `setIsUiVisible`, `setIsMouseInputEnabled`, `versionString` |
| `.ini` ayar dosyaları | `.json` ayar dosyaları |
| CMake hedefi `prime_map` | `prime-map-api`; DLL adı değişmedi |
