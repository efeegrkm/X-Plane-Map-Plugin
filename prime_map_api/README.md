# Prime XMap API 0.26 — X-Plane 11 harita kütüphanesi

Herhangi bir X-Plane 11 eklentisinin kendi arayüzüne katmanlı, hareketli bir harita koymasını sağlayan C++20
kütüphanesi (CMake hedefi `prime-map-api`, çıktı `prime_map.dll`, ad alanı `prime`). Dış harita servisi yok: zemin,
pistler, havalimanı yüzeyleri, navaid'ler, hava yolları ve uçuş planı o bilgisayardaki X-Plane verisinden gelir;
haritada görünen her şey simülatördekiyle aynıdır. Hedef: eğitmen istasyonu (IOS) ekranları ve her türlü X-Plane eklentisi.

**Hedef ve önerilen kullanım: `WindowStyle::Texture`.** Harita kütüphanenin framebuffer'ında bir openGL dokusuna çizilir,
host onu kendi penceresinde / panelinde istediği yere basar ve fare / tekerlek / klavyeyi haritaya iletir. Eski pencere
stilleri (`Standalone`, `Embedded`) **LEGACY**'dir: kodları Texture kapsamlı denenene kadar korunur, yeni kodda kullanılmaz.
Ürün geliştirme aşamasındadır: sürümler arasında geriye uyumluluk sağlanmaz (ayar dosyaları yalnızca JSON).

| Belge | Kimin için |
|---|---|
| `docs/PrimeMap_API.pdf` | **Kullanım kılavuzu ve referans**: hızlı başlangıç, sıfırdan entegrasyon, bütün fonksiyonlar, özelleştirme, dosya biçimleri, sınırlar (kaynağı `docs/tools/build_api_doc.py`) |
| `docs/SRS.md` | Yazılım gereksinimleri: hedef ("API ile haritanın her yerinde tam kontrol") ve her gereksinimin bugünkü durumu |
| `docs/MIMARI.pdf` | Kodu anlamak isteyen geliştirici: mimari, veri akışı, çizim yolları, dosya görevleri |
| `examples/texture_host/` | En küçük tam host eklentisi (derlenir; kılavuzun bölüm 3'ü) |
| `../plugin-api-test/` | Bütün API çağrılarını deneyen test eklentisi; `docs/API_LISTESI.md` kısa çağrı listesi |
| `../prime_map_plugin/` | Resmî eklenti (ince kabuk; şu an LEGACY Standalone) |

## Beş adımda

```cpp
#include <prime_map_api/PrimeMap.h>

prime::XMapManager* tManager = prime::XMapManagerFactory::createXMapManager();
tManager->setDataDirectory(myPluginFolder);        // ayar + onbellek klasoru
tManager->initialize();

prime::XMapConfig tConfig;                         // varsayilan stil: Texture
tConfig.mName = "main";
tConfig.mUiVisible = true;
tConfig.mMouseInput = true;
prime::XMap* tMap = tManager->addXMap(tConfig);
tMap->setTextureSize(600, 400);                    // boxel
tMap->initialize();

// host'un draw callback'i:   if (tMap->renderTexture()) { XPLMBindTexture2d(tMap->getTextureId(), 0); /* dortgen */ }
// host'un girdi callback'leri: tMap->injectMouse / injectMouseMove / injectWheel / injectKey (doku koordinati)
// kapanis:                   tManager->removeXMap(tMap); tManager->shutdown();
//                            prime::XMapManagerFactory::destroyXMapManager(tManager);
```

CMake: `add_subdirectory(<prime_map_api> prime_map_build)` + `target_link_libraries(<hedef> PRIVATE prime-map-api)`;
`prime_map.dll` host'un `win.xpl` dosyasının yanına kopyalanır. CRT `/MD`. Tam akış: kılavuz bölüm 2–3.

## Derleme ve testler

Ön koşul: Visual Studio 2022 (MSVC v143), CMake 3.16+, Ninja ve üçüncü taraf bileşenler (aşağıda). Qt Creator'da projeyi
açıp derlemek de yeter (kit: Desktop Qt 6.10.1 MSVC2022 64bit; Qt kütüphanesi kullanılmaz).

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DMAP_TESTS=ON -DMAP_PREVIEW=ON
ninja -C build
build\map_tests.exe
```

| Seçenek / hedef | Ne |
|---|---|
| `-DMAP_TESTS=ON` → `map_tests` | X-Plane'siz testler (çekirdek: görünüm, katman kuralları, ayarlar, apt.dat, DSF, zemin, ...). Hepsi geçmeli |
| `-DMAP_PREVIEW=ON` → `map_preview`, `dsf_info` | Araçlar (aşağıda) |
| `-DCOTS_DIR=...`, `-DXPSDK=...` | Üçüncü taraf klasörü / X-Plane SDK klasörü |

Sürüm: `src/api/XMapTypes.h` `kVersion*` ile `CMakeLists.txt` `project(VERSION)` configure'da denetlenir; birlikte değiştirin.

## Üçüncü taraf bileşenler

Şirket kuralı gereği üçüncü taraf kodu repoda tutulmaz. CMake sırayla `-DCOTS_DIR`, `C:\workspace\cots` ve kardeş `..\cots`
klasörüne bakar.

| Bileşen | Sürüm | Ne için | İndirme | Klasör |
|---|---|---|---|---|
| X-Plane SDK | 3.0 (XPLM301) | Eklenti arayüzü, `XPLM_64.lib` (kütüphane + host) | https://developer.x-plane.com/sdk/ | `cots/X-Plane-SDK` |
| LZMA SDK | 24.05 | DSF (7z) açma, rakım önbelleği | https://www.7-zip.org/sdk.html | `cots/lzma` |
| earcut.hpp | 2.2.4 | Poligon üçgenleme | https://github.com/mapbox/earcut.hpp | `cots/earcut/mapbox/earcut.hpp` |
| stb_truetype.h | 1.26 | Yazı atlası | https://github.com/nothings/stb | `cots/stb/stb_truetype.h` |

LZMA, earcut ve stb yalnızca kütüphane derlenirken gerekir (DLL'in içine gömülür). Host'un gereksinimi sadece X-Plane SDK.

## Veri klasörü

Yöneticinin veri klasörü: `setDataDirectory()`; verilmezse `%LOCALAPPDATA%\prime\prime-xmap\`. Aynı süreçte birden çok
host varsa her biri kendi klasörünü vermelidir.

| Dosya | Ne |
|---|---|
| `prime_map.json` | Merkezi ayarlar: ortak önbellek sınırları (açık harita sayısıyla büyür), önceden yükleme, otomatik kayıt |
| `maps/<ad>/map_settings.json` | Harita başına: görünüm, mod, katmanlar (mod başına), zoom aralıkları, renkler, kaynaklar |
| `maps/<ad>/terrain_colors.json` | Harita başına zemin renkleri ve ayarları |
| `maps/<ad>/map_drawings.txt` | Kullanıcı çizimleri |
| `cache/terrain/`, `cache/airports/` | Ortak önbellek (silinebilir; karo başına ~3–4 MB, `terrain_mb` aşılınca en eski kullanılan silinir) |

Bütün anahtarlar: kılavuz bölüm 11.

## Harita modları

| Mod | Zemin | Öne çıkanlar |
|---|---|---|
| Scenery | Arazi sınıfları + kabartma (koyu tema) | Genel görünüm |
| VFR | Sectional yükseklik bantları + kabartma, su açık mavi + kıyı çizgisi | Kuleli havalimanı mavi, kulesiz macenta; VOR pusula gülü; Victor yolları |
| IFR Low | Kara / su (iki düz renk) + ince kıyı | Alçak hava yolları, VOR / NDB / ILS / fix |
| IFR High | Aynı | Yüksek hava yolları, sadece büyük havalimanları |
| Dark | Siyah kara, koyu lacivert su | Minimal: havalimanları, pistler, navaid'ler, hava yolları, uçak, rota |

Her modun katman durumu ayrı saklanır; varsayılanlar ve renkler `src/XMapTheme.cpp`'de.

## Katmanlar (özet)

56 öğe (6 grup, katmanlar, alt katmanlar); tam liste, anahtarlar, mod başına varsayılanlar ve renk verilebilirlik: kılavuz bölüm 10.

| Grup | Katman | Yerleşik eşik (zoom) |
|---|---|---|
| XMap | Terrain (Forests, Built-up), Roads and railways (6 alt), Lat / lon grid | 5+ / 7.5+ / her zoom |
| Airports | Airport symbols, Runways (6 alt), Airport surfaces (4 alt), Airport objects (11 alt), Taxi routes | 4+ / 11+ / 10.5+ / 12.5+ / 13.5+ |
| Navaids | VOR / DME, NDB, ILS / LOC, Fixes | 5+ / 7+ / 8.5+ / 9+ |
| Airways | Airways (low) / (high) | 6.5+ (adlar 8+, oklar 9+) |
| Flight | Aircraft, FMS route, Flight track | her zoom |
| — | User drawings (Points, Routes, Regions, Names and distances) | her zoom |

Etiketler öncelikle yer bulur (havalimanı → pist numarası → VOR → NDB → ILS → fix → hava yolu); hiçbir etiket bir sembolün
üstüne binmez.

## Zemin nasıl çalışıyor

- Her 1°×1° karo için sahne paketleri X-Plane'in sırasıyla taranır (`Custom Scenery` / `scenery_packs.ini`, sonra
  `Global Scenery`); **overlay olmayan ilk DSF** karonun zeminidir (Ortho4XP gibi özel mesh'ler de görünür).
- DSF'ler kütüphane içinde açılır, arazi üçgenleri 4096×4096'lık sınıf haritasına boyanır, DEM'den kabartma hesaplanır;
  iş arka planda yapılır, sonuç `cache/terrain/`'e yazılır.
- **Yakın zoom:** ekrandaki küçük parçalar (1/32°, 1/128°) doğrudan DSF üçgenlerinden boyanır; Scenery'de sınıf sınırları
  kıvrılır ve kara sınıfları arasında yumuşak geçiş yapılır.
- **Senkron:** her karo üretildiği DSF'lerin yol + boyut + değişiklik zamanıyla etiketlidir; sahne değişince karo yeniden
  üretilir. Önbelleği silmek hiçbir şeyi bozmaz.
- **Kabartma:** tam çözünürlüklü rakım (1201×1201, ~92 m) sıkıştırılmış tutulur; gölge piksel boyuna uygun ön-ortalanmış
  seviyeden, dört yönlü ışıkla. z15+'da kabartmanın yumuşak görünmesi X-Plane verisinin sınırıdır.
- **Su:** kıyı çizgisi, kıyıdan açığa açık şerit, derinliğe göre koyulaşma (DSF `sea_level`).
- **Deniz / veri yok:** sahnesi kurulu olmayan karo için X-Plane'in dünya tablosuna (`Resources/dsf_files`) bakılır.
- **Ayarlar:** `maps/<ad>/terrain_colors.json` ya da `XMap::getTerrainSettings / setTerrainSettings` (kılavuz 9.4, 11.2).

## Araçlar (X-Plane'siz; `-DMAP_PREVIEW=ON`)

- `map_preview`: kütüphanenin katman kodunu gerçek X-Plane verisiyle bir resme çizer.
  `map_preview --lat 47.45 --lon -122.30 --zoom 11 --mode scenery --cache <klasor>\ --out ksea.ppm`
  (`--show taxi_routes`, `--hide grid`, `--panel Airports`, `--describe x,y`, `--demo-drawings`, `--terrain-colors <json>`).
- `dsf_info`: bir DSF'in özeti (özellikler, dışlamalar, arazi türleri, üçgenler, yol / poligon envanteri); `--raw out.dsf`,
  `--dump-terrain <indeks> out.txt`.

## Bilinen sınırlar

Ayrıntı: kılavuz bölüm 9 (özelleştirme durumu) ve 15; gereksinim karşılığı: `docs/SRS.md`.

- Katman rengi 14 katmanda; `TerrainBuiltUp` rengi kabul edilir ama görünmez (bilinen hata). Çizgi kalınlığı, sembol / yazı
  boyu, tema tanımı yok. Katman zoom aralığı yerleşik eşiği sadece daraltır.
- Texture haritası pop-out edilmiş host penceresinde çizilmez (ayrı GL bağlamı).
- Log öneki süreçte tektir; aynı süreçteki bütün host'lar aynı `prime_map.dll`'yi kullanmalıdır.
- Bina / 3B nesne yok; 180. boylam civarı kısmen; yazılar ASCII; ILS yönü gerçek (true).
