# Prime Map API 0.22 — X-Plane 11 harita kütüphanesi ve gömme API'si

X-Plane içinde, **kendi penceresinde** çalışan, katmanları tek tek açılıp
kapatılabilen haritanın motoru ve gömme API'si (`prime_map` SHARED DLL'i).
Dış harita servisi (Kaşif, OSM) yok: zemin, pistler, havalimanı yüzeyleri,
navaid'ler, hava yolları ve uçuş planı o bilgisayarda kurulu X-Plane verisinden
gelir. Bu yüzden haritada görünen her şey simülatördekiyle aynıdır; yeni sahne
kurulunca harita da kendiliğinden güncellenir. Hedef kullanım: simülatörlerin
eğitmen istasyonu (IOS) ekranları ve herhangi bir X-Plane 11 eklentisine gömme.

> Not: Bu repo **kütüphane + API'dir** (Bitbucket:
> <https://gobitbucket.havelsan.com.tr/scm/eps/prime-map-api.git>). Resmî
> X-Plane eklentisi (ince kabuk) ayrı repodadır:
> <https://gobitbucket.havelsan.com.tr/scm/eps/prime-map-plugin.git>.

| Belge | Kimin için |
|---|---|
| `README.md` (bu dosya) | Kurulum, tüketim, üçüncü taraf bileşenler, testler, araçlar |
| `docs/PrimeMap_API.pdf` (kaynağı `docs/PrimeMap_API.html`) | API'yi kullanacak host geliştirici: hızlı başlangıç, fonksiyon referansı |
| `docs/MIMARI.pdf` (kaynağı `docs/MIMARI.html`) | Kodu anlamak isteyen geliştirici: mimari, veri akışı şemaları, dosya görevleri |
| `AGENTS.md` (repoya girmez; yerel) | Projede çalışacak yapay zekâ asistanı: kurallar, kod standardı, derleme, doğrulanmış gerçekler, durum |

## Prime Map API (0.22)

Harita başka bir X-Plane eklentisine gömülebilir bir C++ kütüphanesi: `prime_map`
(SHARED DLL; tek genel başlık `PrimeMap.h`, repo kökünde; include
`#include <prime_map_api/PrimeMap.h>`). Bu repo yalnızca
kütüphaneyi üretir; resmi X-Plane eklentisi ayrı repodadır (`prime_map_plugin`) ve
haritaya sadece bu API ile erişir; kullanıcı açısından görünüş ve ayar dosyaları
0.19 ile aynı. Ayrıntı ve fonksiyon listesi: `docs/PrimeMap_API.pdf`; bütün
fonksiyonları deneyen ayrı test eklentisi: `examples/map_api_test`.

**0.22'de kütüphane sunumu yenilendi:** `add_library(prime_map SHARED ...)` + export
makrosu `primemapglobal.h` (`PRIME_MAP_EXPORT`) + kurulum fabrikası `MapFactory`
(`Map::Open` geriye uyumluluk için durur ve fabrikaya yönlendirir). SDK paketi/install
**yoktur**; tüketim CMake hedefi üzerinden yapılır (aşağıda "Tüketim").

Eklentinin menüsünden/panellerinden yapılan her şeyin API karşılığı vardır —
yeni bir host bu fonksiyonlarla aynı davranışı kurabilir:

| Neyi | API |
|---|---|
| Pencere | `MapFactory::CreateMap` (veya `Map::Open`), `SetGeometry`, `SetVisible`, `SetPoppedOut`, `SetUiVisible`, `SetMouseInput` |
| Görünüm | `SetCenter`, `SetZoom`, `ZoomIn/Out`, `Pan*`, `FitBounds`, `SetFollowAircraft`, `GoToAirport`, `FindAirports` |
| Mod | `SetMode(mode)`, `NextMode()`, `GetMode()` |
| **Katmanlar** | `SetLayerVisible(MapLayer, bool)`, `ToggleLayer`, `ShowOnlyLayers`, `SetLayersVisible`, `ResetLayersToDefaults`, `GetLayerState` (ağaç: `TopLevelLayers`, `ParentOf`, `ChildrenOf`) |
| Çizimler | `SetDrawTool`, `AddDrawingPoint`, `FinishDrawing`, `UndoDrawing`, `Export/ImportDrawings` |
| Uçak / rota | `SetAircraftSource`, `SetAircraft`, `SetRouteSource`, `SetRoute`, `ClearTrack` |
| Ayarlar | `ExportSettings` / `ImportSettings`, `Get/SetTerrainSettings`, revizyon sayaçları |

## Kurulum ve derleme

**Ön koşul — üçüncü taraf bileşenler:** aşağıdaki "Üçüncü Taraf Bileşenler"
bölümündeki bileşenler `C:\workspace\cots` altına kurulmuş olmalı (repoda
tutulmazlar; şirket COTS kuralı).

Qt Creator'da projeyi aç ve derle (kit: Desktop Qt 6.10.1 MSVC2022 64bit; Qt
kütüphanesi kullanılmıyor, sadece MSVC + CMake + Ninja).

Komut satırından:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DMAP_TESTS=ON -DMAP_PREVIEW=ON
ninja -C build
```

- CMake seçenekleri: `-DMAP_TESTS=ON` (X-Plane'siz testler, `map_tests.exe`),
  `-DMAP_PREVIEW=ON` (araçlar, aşağıda), `-DCOTS_DIR=...` (farklı COTS klasörü),
  `-DXPSDK=...` (farklı X-Plane SDK klasörü).
- Derleme çıktıları: `prime_map.dll` (+ import kütüphanesi `prime_map.lib`),
  `map_tests`, `map_preview`, `dsf_info`, örnek eklenti `examples/map_api_test`.
- CRT: `/MD` (`CMAKE_MSVC_RUNTIME_LIBRARY` sabittir, seçenek yoktur).
  Tüketiciler de `/MD` ile derlenmek zorundadır.

### Prime Map API'yi kendi eklentine kurma (adım adım)

SDK paketi yoktur; kütüphane, tüketicinin **aynı CMake oturumuna** katılır ve
hedef adına linklenir (prime-builder'ın `primesceneryparser` tarafından
tüketilmesiyle aynı mantık). Sıfırdan bir host için bütün akış:

**Adım 0 — Ön koşullar.** Visual Studio 2022 (MSVC v143), CMake 3.16+, Ninja
(Qt Creator kuruluysa `C:\Qt\Tools\Ninja\ninja.exe` hazırdir) ve
`C:\workspace\cots` altında X-Plane SDK 3.0 (aşağıda "Üçüncü Taraf
Bileşenler"). CRT sabit `/MD`'dir; host da `/MD` ile derlenir.

**Adım 1 — Repo yerleşimi.** Bu repoyu, kendi eklenti projenin **kardeş
klasörü** olarak al (klasör adı include öneki olur, değiştirmeyin):

```
C:\workspace\projects\
├── prime_map_api\        <- bu repo (kutuphane)
└── benim-eklentim\       <- senin projen
```

```bat
git clone https://gobitbucket.havelsan.com.tr/scm/eps/prime-map-api.git C:\workspace\projects\prime_map_api
```

Farklı bir yer de mümkündür; 2. adımda yolu vermek yeterli.

**Adım 2 — CMakeLists.txt'ye üç şey** (tam örnek: `prime_map_plugin/CMakeLists.txt`):

```cmake
# (a) Kutuphane klasoru (cache degiskeni; -DPRIME_MAP_DIR ile devralinabilir)
set(PRIME_MAP_DIR "${CMAKE_SOURCE_DIR}/../prime_map_api" CACHE PATH "prime_map_api klasoru")
if(NOT EXISTS "${PRIME_MAP_DIR}/PrimeMap.h")
    message(FATAL_ERROR "prime_map_api bulunamadi: ${PRIME_MAP_DIR}\n"
                        "Reponun eklenti projenin KARDESI olarak klonlanmis olmasi gerekir.")
endif()

# (b) Kutuphaneyi derleme oturumuna kat (ikinci arguman build klasorudur)
add_subdirectory(${PRIME_MAP_DIR} prime_map_build)

# (c) Hedefine linkle: include dizini ve /MD otomatik gelir
target_link_libraries(benim-eklentim PRIVATE prime_map)
```

`target_link_libraries` tek satır yeter: `prime_map` hedefi include
dizinini (`<prime_map_api/PrimeMap.h>` yazabilmek için gereken) ve CRT
ayarını kendisi yayınlar. `prime_map.dll` derlendiğinde senin
`CMAKE_RUNTIME_OUTPUT_DIRECTORY`'ne düşmez — 4. adımdaki kopya gerekir.

**Adım 3 — İlk kod.** Giriş noktası `MapFactory::CreateMap`'tir
(`Map::Open` geriye uyumluluk için durur ve fabrikaya yönlendirir):

```cpp
#include <prime_map_api/PrimeMap.h>   // klasor adi onekli (prime-utils/utils.h mantigi)

PrimeMap::Map* map = PrimeMap::MapFactory::CreateMap("benim-eklentim", *host);
if (!map) return;                      // olusmadiysa Log'a bakinin
map->SetGeometry(50, 50, 900, 600);    // sol-ust 50,50; 900x600 (boxel)
map->SetVisible(true);                 // ilk karede gorunur olur
```

Kurallar: bütün çağrılar X-Plane ana thread'inden; pencere `XPLMCreateWindow`
ile host'a değil kütüphaneye ait olur; istisna yoktur — geçersiz değer
işlem görmez, `false`/boş döner, Log'a tek satır yazar (tam fonksiyon
listesi: `docs/PrimeMap_API.pdf`).

**Adım 4 — Derleme ve dağıtım.** `ninja`, `prime_map.dll`'yi API build
klasöründe üretir; host eklentinin **yanına kopyalanması gerekir** (Windows
DLL arama kuralı + X-Plane eklenti klasörü yapısı). POST_BUILD ile otomatikleştir:

```cmake
add_custom_command(TARGET benim-eklentim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:prime_map> $<TARGET_FILE_DIR:benim-eklentim>)
```

X-Plane kurulumunda **ikisi birlikte** gider:
`Resources/plugins/<host>/64/win.xpl` + `64/prime_map.dll` (X-Plane, eklenti
klasörünü DLL arama yoluna aldığı için oradan yüklenir). `/MD` nedeniyle hedef
makinede MSVC 2022 redistributable (`VC_redist.x64`) kurulu olmalıdır.

**Adım 5 — Doğrulama.** X-Plane'i aç; Log.txt'de
`[prime-map-plugin] Prime Map API 0.22 ready` benzeri satırı gör. Kod
tarafında `PrimeMap::VersionString()` de `"0.22.0"` döner. Sürüm sabitleri
(`PrimeMap.h` içindeki `kVersionMajor/Minor/Patch`) CMake'in
`project(VERSION)`'u ile configure sırasında denetlenir; biri
güncellenmezse configure **hata verir** (sürüm attırırken ikisini birlikte
değiştir).

**Sorun giderme:**

| Belirti | Neden / çözüm |
|---|---|
| `prime_map_api bulunamadi` | Kardeş klasör yok veya `-DPRIME_MAP_DIR` yanlış |
| `PrimeMap.h: No such file` | `target_link_libraries` eksik ya da add_subdirectory çağrılmadan include kullanıldı |
| Linkte `unresolved external symbol` (PRIME_MAP_EXPORT'lu) | Host `/MT` ile derleniyor; `/MD`'ye geçir (kütüphane `/MD` sabittir) |
| X-Plane'de eklenti yüklenmiyor, Log'da DLL hatası | `prime_map.dll` `64/` klasöründe değil, ya da eski sürüm (yeniden kopyala) |
| Hedef makinede çalışmıyor, geliştiricide çalışıyor | VC_redist.x64 eksik |

## Üçüncü Taraf Bileşenler

Şirket kuralı gereği üçüncü taraf kaynak kodu/binary'leri repoda **tutulmaz**;
`C:\workspace\cots` altına kurulur ve CMake oradan okur. Aşağıdaki bileşenleri
indirip ilgili klasörlere açın:

| Bileşen | Sürüm | Ne için kullanılıyor | İndirme | Kurulum klasörü |
|---|---|---|---|---|
| X-Plane SDK | 3.0 (XPLM301) | X-Plane eklenti arayüzü: pencere, çizim, dataref, komut başlıkları ve `XPLM_64.lib` | https://developer.x-plane.com/sdk/ | `C:\workspace\cots\X-Plane-SDK` |
| LZMA SDK | 24.05 | DSF dosyaları 7z ile sıkıştırılır; zemin okuma/yazma | https://www.7-zip.org/sdk.html | `C:\workspace\cots\lzma` |
| earcut.hpp | 2.2.4 | Poligon üçgenleme (bölge çizimleri) | https://github.com/mapbox/earcut.hpp | `C:\workspace\cots\earcut` (içi `mapbox/earcut.hpp`) |
| stb_truetype.h | 1.26 | Yazı atlası (TrueType okuma) | https://github.com/nothings/stb | `C:\workspace\cots\stb` |

Not: LZMA, earcut ve stb yalnızca **kütüphaneyi derlerken** gerekir; kütüphanenin
içine gömülüdür. Host'un gereksinimi sadece X-Plane SDK 3.0 başlıklarıdır
(bkz. `docs/PrimeMap_API.pdf`).

Log: `C:\X-Plane 11\Log.txt` içinde `[prime-map-plugin]` ile başlayan satırlar.

## Kullanım

**Menü: Plugins → Prime Map Plugin**

| Öğe | Komut (Settings → Keyboard'da tuşa atanabilir) |
|---|---|
| Show / hide map | `prime/map/toggle` |
| Show / hide controls | `prime/map/controls` |
| Next map mode | `prime/map/next_mode` (Scenery → VFR → IFR Low → IFR High → Dark → ...) |
| Show / hide X-Plane map (compare) | X-Plane'in `sim/map/show_current` komutu: yan yana karşılaştırmak için |
| Diagnostics mode | `prime/map/diagnostics` |
| Clear flight track | — |
| Pre-cache terrain of visible area | `prime/map/precache_view`: haritada görünen alandaki kurulu karoların zemin önbelleğini (raster, rakım, vektör) arka planda hazırlar; ilerleme alt şeritte. Önbellek sınırının %80'ini aşacaksa başlamaz |
| Cancel terrain pre-cache | `prime/map/precache_cancel` |

**Fare**
- Sürükle: haritayı kaydırır (her modda; uçak takibini kapatır).
- Tekerlek: zoom (en çok 16.5); imlecin altındaki nokta yerinde kalır
  (takipteyken uçağın etrafında). İmleç katman panelindeyse paneli kaydırır.
- Point / Route / Region aracında: **tıkla** = nokta / köşe ekle, **sağ tık** =
  rota/bölgeyi bitir.
- Fareyi bir nesnenin üstünde tut: kısa açıklama (pist, yüzey, kapı, navaid ...).

**Üst şerit**

| Düğme | Ne yapar |
|---|---|
| Layers | Katman paneli: grup › katman › alt katman. Kutu altındakilerin hepsini açar/kapatır (karışıksa çizgi), ok listeyi açar (varsayılan kapalı). Başlıktaki **Defaults** bu modun bütün katmanlarını varsayılana döndürür |
| Map: ... | Harita modu listesi (aşağıda) |
| Follow aircraft / Center on aircraft | Uçağı takip et / bir kez uçağa ortala |
| Go to airport | Arama paneli: ICAO, kimlik veya ad yaz (ör. `KSEA`, `tacoma`), en iyi 8 sonuçtan birine tıkla ya da ↑/↓ + Enter. Harita oraya gider ve havalimanı ekrana sığar; **uçağın konumu değişmez**, uçak takibi kapanır. Panel açıkken klavye haritadadır (tuşlar X-Plane'e gitmez); haritaya tıklayınca ya da Esc ile klavye X-Plane'e döner |
| Draw tools | Çizim paletini göster / gizle |
| Pop out / Pop in | Pencereyi ayrı işletim sistemi penceresine al (ör. ikinci monitör) / geri koy |
| Hide | Kontrolleri gizle (imleç haritaya gelince sol üstte **Controls** düğmesi çıkar) |

**Çizim paleti** (haritanın üstünde yüzer): Pan / Point / Route / Region ve
Finish / Undo / Clear. Başlığından (`:: DRAW`) tutup taşınır, ok ile seçili
araca katlanır; yeri ve durumu kaydedilir.

**Alt şerit:** imlecin enlem/boylamı, zoom, ölçek çubuğu (NM) ve durum yazısı
(yükleme, çizim ipucu, teşhis modunda FPS). Ekranda istenen kalitede olmayan
(kaba ya da henüz boş) zemin olduğu sürece `Loading terrain: N images` yazar.

## Harita modları

**Map: ...** düğmesi veya `prime/map/next_mode`. Her modun katman ve alt katman
açık/kapalı durumu ayrı saklanır. Varsayılanlar (0.18) kullanıcının X-Plane'de kaydettiği ayarlardır
(`src/MapTheme.cpp` başındaki tablolar); `map_settings.ini`'deki değer her zaman önceliklidir. Renkler `src/MapTheme.cpp`'de (ilk sürüm FAA
harita geleneklerinden; X-Plane'in kendi haritasıyla karşılaştırılıp
ayarlanacak).

| Mod | Zemin | Öne çıkanlar |
|---|---|---|
| Scenery | Arazi sınıfları + kabartma (koyu tema) | Genel görünüm |
| VFR | Sectional yükseklik bantları (0 / 1000 / 2000 / 3000 / 5000 / 7000 / 9000 / 12000+ ft) + kabartma, su açık mavi + kıyı çizgisi | Kuleli havalimanı mavi, kulesiz macenta; VOR pusula gülü; Victor yolları kalın açık mavi; ILS ve fix kapalı |
| IFR Low | Sadece kara / su (iki düz renk) + ince kıyı çizgisi | Alçak hava yolları, VOR/NDB/ILS/fix; büyükler yeşil, diğerleri kahverengi |
| IFR High | Aynı | Yüksek hava yolları, sadece büyük havalimanları; NDB, ILS, alçak yollar kapalı |
| Dark | Tam siyah kara, koyu lacivert su (kabartma, sınıf, yol yok) | Uçuş kontrol ekranı tarzı minimal: havalimanları, pistler, VOR / NDB / ILS / fix, hava yolları, uçak ve rota |

VOR gülü manyetik kuzeye döner (sapma X-Plane'in VOR kaydından; Log'da
`VOR sample:` satırları).

## Katmanlar

Alttan üste çizim sırası. Etiketler bir öncelikle yer bulur (havalimanı → pist
numarası → VOR → NDB → ILS → fix → hava yolu); hiçbir etiket bir sembolün üstüne
binmez, sığmayan etiket zoom yapınca görünür.

| Grup | Katman | Görünür | Alt katmanlar / not |
|---|---|---|---|
| Map | Terrain (scenery) | zoom 5+ | Kurulu DSF'lerden; biçim moda göre. Alt katmanlar: Forests (DSF orman poligonları, Scenery), Built-up areas (yerleşim; VFR'da sarı) |
| Map | Roads and railways | 7.5+ | DSF yol ağı: Motorways 7.5+, Primary 9+, Secondary 10.5+, Local 12+, Railways 9.5+, Power lines 11+ (varsayılan kapalı). IFR'da varsayılan kapalı |
| Map | Lat / lon grid | her zoom | Aralık zoom'a göre |
| Airports | Airport symbols | 4+ | Büyük 4+, küçük 8+, deniz üssü 9+, heliport 11+ (12.5+'da gerçek helipad), kapalı 11+ |
| Airports | Runways | 9+ | Paved, Unpaved, Water runways, Blast pads / overruns (12+), Markings (13+), Runway numbers (11.5+) |
| Airports | Airport surfaces | 10.5+ | Pavement (11+), Pavement edges, Airport boundary (10.5+), Painted lines (13.5+) |
| Airports | Airport objects | 12.5+ | Edge / taxi lights (14.5+), Helipads, Beacons (12.5+), Windsocks (13.5+), Gates, Tie-downs, Hangars, Other start locations (14+), Parking names (15.5+), PAPI / VASI (14.5+), Taxi signs (15.5+) |
| Airports | Taxi routes (varsayılan kapalı) | 13.5+ | Taksi ağı (1201/1202), pist üstü kısım kırmızı, taksi yolu adları |
| Navaids | VOR / DME, NDB, ILS / LOC, Fixes | 5+ / 7+ / 8.5+ / 9+ | ILS tüyü pist eşiğinden |
| Airways | Airways (low) / (high) | 6.5+ | earth_awy.dat; adlar 8+, yön okları 9+ |
| Flight | Aircraft, FMS route, Flight track | her zoom | |
| — | User drawings | her zoom | Points, Routes (NM), Regions (NM²), Names and distances |

## Zemin (Terrain) nasıl çalışıyor

- Eklenti, X-Plane'in yaptığı gibi her 1°×1° karo için sahne paketlerini öncelik
  sırasıyla tarar: önce `Custom Scenery` (scenery_packs.ini'deki etkin sıra),
  sonra `Global Scenery`. **Overlay olmayan ilk DSF** karonun zeminidir
  (Ortho4XP gibi özel bir mesh kurarsan onun zemini görünür).
- DSF'ler 7z ile sıkıştırılmıştır; eklenti kendisi açar, arazi üçgenlerini
  4096×4096'lık bir sınıf haritasına boyar ve DEM'den kabartma hesaplar. Bu iş
  arka planda yapılır, sonuç `prime-map-plugin\cache\terrain\` altına yazılır
  (~2 MB/karo).
- **Yakın zoom:** yaklaşınca zemin raster'dan büyütülmez, ekrandaki küçük
  parçalar (1/32° ve 1/128°) doğrudan DSF üçgenlerinden boyanır; Scenery
  modunda arazi sınırları doğal biçimde kıvrılır ve kara sınıfları arasında
  yumuşak geçiş yapılır (su ve kıyı keskin kalır).
- **Senkron kalma:** önbellekteki her karo, üretildiği DSF'lerin
  yol + boyut + değişiklik zamanı listesiyle etiketlidir. Sahne eklenir,
  çıkarılır, sırası değişir veya güncellenirse karo yeniden üretilir. Önbelleği
  silmek hiçbir şeyi bozmaz.
- **Kabartma (0.16):** DSF'in tam çözünürlüklü rakımı (1201×1201 nokta, ~92 m)
  önbellekte sıkıştırılmış tutulur (`*.elevation`, ~0,75 MB/karo). Gölge her
  pikselde, piksel boyuna uygun bir ön-ortalanmış seviyeden hesaplanır: uzakta
  parazitsiz, yakında bloksuz. Işık dört yönden (ağırlık kuzeybatıda); dik
  yamaçlar biraz koyu. Komşu karo hazır olunca karo sınırındaki dokular
  kendiliğinden yenilenir (dikiş kalmaz). **Beklenti:** rakım verisi ~92 m
  aralıklı olduğu için z15+ zoomda kabartma yumuşak görünür; bu X-Plane
  verisinin sınırı (simülatörün zemini de aynı veriden).
- **Su (0.16):** kara/su sınırında suyun tarafında tek piksel kıyı çizgisi (her
  modda); Scenery ve VFR'da kıyıdan açığa ~200 m açık şerit ve açık denizde derinliğe
  göre koyulaşma (DSF'in `sea_level` verisinden; verisi olmayan yerde düz). Kıyının
  şekli değişmez, simülatördekiyle aynıdır.
- **Deniz / veri yok (0.16):** sahnesi kurulu olmayan karo için X-Plane'in dünya
  tablosuna (`Resources/dsf_files`) bakılır: orada DSF'i olmayan karo **deniz**,
  olan karo **veri yok** (kara ama kurulu değil; nötr gri, deniz gibi boyanmaz).
  Tablo yoksa veya doğrulanamazsa eski kural: 10°×10° klasörü bir Global Scenery
  paketinde kuruluysa deniz, değilse veri yok.
- **Renkler ve ayarlar:** `prime-map-plugin\terrain_colors.ini` (ilk açılışta
  varsayılanlarla oluşur, X-Plane kapalıyken düzenle):

| Bölüm | Anahtar | Ne |
|---|---|---|
| `[colors]` | `<sınıf> = R G B` | Sınıf renkleri (0–255) |
| `[shading]` | `strength` | 0 (düz) … 1 (güçlü kabartma) |
| `[blend]` | `meters` (150), `max_pixels` (5) | Scenery'de kara sınıfları arası yumuşak geçiş (0 = kapalı); yakın zoom'da doku pikseli tavanı |
| `[warp]` | `meters` (65), `wavelength` (250) | Sınıf sınırlarının kıvrılması (0 = düz üçgen kenarları) |
| `[closeup]` | `class_mix` (0.3) | Yakın zoom'da sınıf renklerinin ortak kara tonuna yaklaşması (0 = kapalı) |
| `[relief]` | `multi_directional` (1), `exaggeration` (2.5), `coarse_boost` (0.35), `slope_tone` (0.12), `flat_slope` (0.02) | Dört yönlü ışık (0 = tek ışık); eğim abartması; uzakta abartma artışı; dik yamaç koyuluğu; düz ovada 1 m'lik rakım basamaklarını gizleyen eşik (0 = kapalı) |
| `[water]` | `coastline` (1), `coastline_strength` (0.5) | Kıyı çizgisi |
| `[water]` | `band_meters` (200), `band_max_pixels` (24), `band_strength` (0.10) | Kıyıdan açığa açık şerit (0 = kapalı) |
| `[water]` | `depth_tone` (0.25), `depth_full_meters` (200) | Derinliğe göre koyulaşma (0 = kapalı) |
| `[vfr]` | `bands` (1), `contour_lines` (0), `contour_strength` (0.35) | Sectional bantları (0 = eski sürekli rampa); bant sınırında ince kontur |
| `[vfr]` | `band_1` … `band_8` = `<alt sınır feet> R G B` | Bant sınırları ve renkleri (sınırlar artan olmalı, değilse varsayılan) |
| `[palette]` | `source` (xplane), `version` | `xplane`: sınıf renkleri X-Plane'in kendi arazi dokularından (ilk açılışta hesaplanır, sonra önbellekten); `ini`: `[colors]` değerleri kullanılır. `version`'ı elle değiştirme |

  Dosyayı silersen varsayılan renkler geri gelir.

## Eklenti klasöründeki dosyalar

| Dosya | Ne |
|---|---|
| `64/win.xpl` | Eklenti |
| `map_settings.ini` | Pencere yeri/boyutu, görünüm, seçili mod, çizim paleti, mod başına katman ve alt katmanlar; `[cache] terrain_mb` (zemin önbelleği sınırı, varsayılan 1024; 0 = sınırsız), `[cache] prefetch_tiles` (önceden yükleme: uçağın önü + FMS rotası, en çok 20 karo; 0 = kapalı); `[render] cache` (1: harita resmi önbelleği / FBO, 0: her karede doğrudan çizim), `[render] max_fps` (resmin en çok yeniden çizilme hızı, 15); `[text] renderer` (`atlas`: kendi yazı çizicimiz, `xplane`: X-Plane'in yazısı) |
| `map_drawings.txt` | Kullanıcı çizimleri (metin; elle düzenlenebilir) |
| `terrain_colors.ini` | Zemin renkleri ve ayarları (yukarıda) |
| `cache/terrain/` | Zemin önbelleği (silinebilir): `*.terrain` sınıflar ve alanlar, `*.elevation` rakım, `*.vectors` DSF vektörleri, `xplane_palette.txt`. Karo başına ~3–4 MB; `terrain_mb` aşılınca en uzun süredir kullanılmayan karolar (tüm dosyalarıyla) silinir, toplam sınırın %90'ına iner |
| `cache/airports/` | apt.dat indeks önbelleği (silinebilir) |

## Test listesi (0.19) — X-Plane'de denenecek

Önce 0.18'de (şu anki kurulu sürüm değilse atla) Diagnostics mode açıp aynı yerde FPS ve `draw ... ms` değerini not et; sonra 0.19 ile karşılaştır.

| # | Ne yap | Beklenen |
|---|---|---|
| 1 | X-Plane'i aç | Log'da `loaded (version 0.19)`, `text: font atlas from ...DejaVuSans.ttf`; hata satırı yok (`map image cache (FBO) not available` çıkarsa bildir) |
| 2 | Yazılara bak (etiketler, düğmeler, paneller) | Okunur, boyutlar eskisine yakın; bulanık değil. Sorun varsa `[text] renderer = xplane` ile eskisine dön ve ekran görüntüsü gönder |
| 3 | Haritayı sürükle, tekerlekle zoom yap | Akıcı; kenarlarda boşluk yok. Zoom anında bir an hafif bulanık, hemen keskinleşir |
| 4 | Follow aircraft, uçarken | Harita akıcı kayar, uçak sembolü ve iz her karede güncel |
| 5 | Pop out, sonra Pop in | Harita iki durumda da çiziliyor, çökme yok |
| 6 | Diagnostics mode | Alt şeritte `map renders N/s`: dururken ~1, kaydırırken ≤ 15. FPS'i 0.18 ile karşılaştır |
| 7 | Katman aç/kapat, mod değiştir, çizim yap | Değişiklik anında görünüyor; rota çizerken imlece giden kesikli çizgi akıcı |
| 8 | Seattle'da zoom 11, Plugins → Prime Map Plugin → Pre-cache terrain of visible area | Altta `Pre-caching terrain: n/N`; bitince `Terrain pre-cache finished`. Cancel menüsü çalışıyor |
| 9 | Zoom 6'da (Seattle bölgesi) pre-cache dene | `area too large` uyarısı (tam sürümde) ya da kurulu karolar kadar iş |

## Test listesi (0.18)

| # | Ne yap | Beklenen |
|---|---|---|
| 1 | `map_settings.ini`'yi yedekleyip sil, X-Plane'i aç | Log'da `loaded (version 0.18)`; her modda katmanlar senin kaydettiğin gibi açılıyor; dosyada `[cache] terrain_mb=1024` |
| 2 | Map: Dark | Kara siyah, su lacivert; havalimanları, pistler, navaid'ler, ILS, hava yolları var; ızgara, yol, havalimanı nesneleri yok |
| 3 | Go to airport → `KBFI` yaz, Enter | Harita Boeing Field'a gider, havalimanı ekrana sığar, uçak yerinde kalır; klavye X-Plane'e döner |
| 4 | Paneli aç, `tacoma` yaz, ↑/↓ ve tıklama | Ad ile arama; satır seçimi ve tıklayınca gitme |
| 5 | Panel açıkken haritaya tıkla, sonra bir X-Plane tuşuna bas | Tuş X-Plane'de çalışır (panel açık kalır) |
| 6 | Hızlı zoom yap (8 → 13) | Kaba / boş zemin göründüğü sürece altta `Loading terrain: N images`; ince doku gelince kaybolur |
| 7 | `terrain_mb=50` yap, X-Plane'i aç | Log'da `terrain cache: ... removed N least recently used tiles`; sonra değeri geri al |

## Test listesi (0.17)

| # | Ne yap | Beklenen |
|---|---|---|
| 1 | X-Plane'i aç | Log'da `loaded (version 0.17)`, `terrain palette: averaged ... X-Plane terrain textures` (ikinci açılışta `from cache`), `terrain colors file updated (palette version 3)` |
| 2 | Seattle zoom 11 (Scenery) | Karolar bir kez yeniden üretilir (`built ... vectors N areas M lines`); yollar görünür, renkler X-Plane tonlarında |
| 3 | Zoom 13 doğu tepeleri | Orman sınırları ayrıntılı, yerel sokaklar görünür; havalimanında orman / yol yok (dışlama) |
| 4 | Map: VFR | Yollar koyu gri, demiryolu siyah, yerleşim sarı; orman yok |
| 5 | Layers → Map → Roads and railways / Terrain alt katmanları | Tek tek açılıp kapanıyor; Power lines açılınca enerji hatları |
| 6 | Map: IFR Low | Yol ve yerleşim kapalı (panelden açılabilir) |
| 7 | Diagnostics, zoom 12–14'te kaydır | FPS'i ve Log'daki `terrain images` satırlarını gönder |

Sorun olursa: Diagnostics mode açıp aynı şeyi tekrarla ve Log.txt'yi gönder.

## Araçlar (X-Plane'siz; `-DMAP_PREVIEW=ON`)

- `map_preview`: eklentinin kendi katman kodunu gerçek X-Plane verisiyle bir
  resme çizer. `map_preview --lat 47.45 --lon -122.30 --zoom 12 --out ksea.ppm
  [--mode vfr] [--show taxi_routes] [--hide grid] [--panel Airports]
  [--describe x,y] [--demo-drawings] [--terrain-colors terrain_colors.ini]`
  (`magick ksea.ppm ksea.png` ile PNG).
- `dsf_info`: bir DSF'in özeti (özellikler, dışlama alanları, arazi türleri,
  üçgen sayıları, yama bayrağı × düzlem sayısı, nesne / poligon / yol ağı
  envanteri, aşama süreleri); `--raw out.dsf` ile açılmış hâlini,
  `--dump-terrain <indeks> out.txt` ile bir arazi türünün üçgenlerini yazar.

## Bilinen sınırlar

- Bina ve 3B nesneler (DSF overlay) yok; havalimanı yüzeyleri apt.dat'tan.
- Zemin önbelleği diskte büyüyebilir (~2 MB/karo, dolaşılan her karo için bir kez).
- Mod renkleri X-Plane'in haritasından örneklenmedi; MEF rakamları ve hava sahası
  sınırları yok.
- ILS yönü gerçek yön (T): X-Plane 11 SDK'sı manyetik yönü vermiyor.
- 180. boylam civarı kısmen ele alınıyor.
- Yazılar XPLMDrawString (tek font, Türkçe harf yok).
- Dokunmatik ekran hareketleri (pinch) yok; dokunuş fare tıklaması gibi gelir.
