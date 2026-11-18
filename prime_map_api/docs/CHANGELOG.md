# Prime Map API — CHANGELOG

## 1.0.0 (MVP yayını, 2026-11-18)

map_tests 34.706 / 0 (5×). Öz-test'e Dark varsayılanı, zoom kırpma / ezme /
temizleme ve sürüm denetimi adımları eklendi. X-Plane kabul testleri
(SRS bölüm 4, test 1–11) bekleniyor.

- **Varsayılan mod Dark**: yeni haritalar `XMapConfig::mMode` ve
  `XMapWindow` katman durumu dahil Dark açılır (0.26.2'de Scenery idi);
  host eski davranış için `mMode = Vfr` vermeye devam edebilir. İkinci
  haritanın modu Vfr; texture host'un eski `mMode` ayarı kaldırıldı.
- **Katman zoom modeli (ZOOM-01/02)**: yerleşik görme eşikleri ve ölçülmüş
  sert alt sınırlar tek tabloda toplandı (`engine/LayerTable.cpp`
  `kZoomTable`, `applyBuiltInZoomRanges` kurulumda uygular). Host aralığı
  (`setLayerZoomRange`) artık yerleşik eşiğin **yerine geçer**: yukarı
  çekmek serbest, aşağı çekmek katman başına sert alt sınıra
  (`getLayerHardMinZoom`) kadar serbest; altındaki istek kırpılır ve tek
  satır uyarı yazılır (örn. pistler 3.0 isteği → 10.0; hava yolları host
  5.0 verince zoom 5'ten çizilir). `(0, 99)` vermek host aralığını
  kaldırır (yerleşik eşiğe döner); `min > max` reddedilir. Yeni API:
  `getDefaultLayerZoomRange`, `getLayerHardMinZoom`; kalıcılıkta yalnız
  host değerleri saklanır. `map_preview`'a host eşiği düşürmek için
  `--layer-zoom <anahtar>:<min>[-<max>]` eklendi.
- **Eşik maliyeti ölçümü (NF-PRF-06)**: `map_preview` KSEA/HNL ölçümü
  (3 tekrar, 0.26.2 stash taban çizgisiyle karşılaştırmalı): yeni eşikler
  tek kare maliyetini artırmıyor — iki tarafta da ~9–12 ms, fark ölçüm
  gürültüsü içinde (headless önizleme ortamının tek kare tabanı ~9–12 ms;
  8 ms hedefi bu ortamın tabanında yok, X-Plane içi ölçüm kabul testi 6'da).
- **Haritasız ön yükleme (TER-05)**: açık harita yokken (ya da harita henüz
  `initialize` edilmeden) kütüphane dataref'ten uçak konumunu okuyup
  15..120 NM ileri karoları arka planda yükler; ilk harita açılışı sıcak
  önbellekle başlar. Tek satır Log yazar; `setPrefetchTiles(0)` tüm ön
  yüklemeyi kapatır.
- **Atomik yazım dayanıklılığı (NF-REL-06)**: geçici dosya adı artık
  benzersiz (`<yol>.<pid>.<iş>.<sayaç>.tmp`) — iki iş parçacığı aynı
  hedefe yazarken birbirini ezmez; yeniden adlandırma okuyucu kilidi
  (Windows ACCESS_DENIED) karşısında 10 denemeye kadar geri çekilerek
  tekrarlar; CacheTrim çökerten kalan 1 saatten eski geçici dosyaları
  siler (Log'da tek satır). Yeni test `testAtomicFile`: 2 iş × 1000 yazım,
  retry'li okuyucuyla 3× temiz.
- **Sürüm denetimi (LIF-09)**: `createXMapManager` kasıtlı olarak inline
  derlenir — sürüm uyuşmazlığı host tarafında yakalanır ve tek satır
  WARNING yazar (`host built with X, loaded DLL is Y at <yol>`); host
  `isLibraryVersionMatching()` (inline, DLL çağrısı gerektirmez) ile
  önceden denetleyebilir. Ready satırı yüklü DLL'in yolunu da verir:
  `Prime Map API 1.0.0 ready (data: ..., cache: ..., dll: ...)`
  (`libraryPath()`, `GetModuleHandleExW` + `GetModuleFileNameW`).
- **Sürüm 1.0.0**: kütüphane ve resmî eklenti 1.0.0; eklenti sürümü
  CMake'ten (`PRIME_MAP_VERSION`) gelir. MVP kapsamı: yukarıdakiler +
  kabul testleri; sürümler arasında geriye uyumluluk sağlanmaz.
- **Kılavuz ve mimari**: 1.0.0 içeriğiyle (yeni zoom modeli, haritasız ön
  yükleme, atomik yazım, sürüm denetimi, eşik/sert sınır tabloları,
  0.26.x → 1.0.0 geçişi) yeniden üretildi; SRS gereksinimleri güncellendi.

## 0.26.2 (ömür döngüsü güvenliği, zemin rengi düzeltmesi, 2026-11-18)

map_tests 30.690 / 0. X-Plane'de denenecek (öz-test'e LIF-08 adımı eklendi).

- **`shutdown` sonrası güvenli no-op (LIF-08)**: yönetici `shutdown()` ettikten
  sonra halâ tutulan `XMap` işaretçileri çökmez; tüm genel metotlar no-op olur
  (ilk çağrıda tek satır Log uyarısı, sonra sessiz; `false` / boş / varsayılan
  döner). `removeXMap` / yıkıcı sonrası işaretçi geçersizdir (korunamaz).
- **`setColor(TerrainBuiltUp)` görüntüye işler (H-01)**: ezme rengi zemin
  paletine `[built_up]` bölümü olarak yazılır ve `composeImage` yerleşim
  karışımında kullanılır; palet ezmesi yoksa dosya biçimi değişmez.
- **Önbellek yazımları atomik**: altı yazıcı noktası (karo, yükseklik, palet
  kaynağı, raster, vektör, AptDat) geçici dosya + yeniden adlandırma ile
  yazar; yarım yazılmış dosya asla kalmaz.
- **İki yönetici uyarısı**: aynı veri klasörüne çözülen ikinci yönetici
  `initialize`'da tek satır WARNING; yalnızca önbellek klasörü ortaksa tek
  satır bilgi yazar.
- **Sarsak testler sabitlendi (H-04)**: beklemeler koşullu `waitUntil`
  yardımcısına çevrildi (30 sn üst sınır); 20× art arda + CPU yükü altında
  doğrulandı.
- **Kılavuz**: 14b "Klasörler ve paylaşım senaryoları" bölümü eklendi;
  `shutdown` sonrası davranış belgelendi; referans tablosuna eksik fonksiyon
  adları girildi; PDF (28 sayfa) yeniden basıldı.

## 0.26.1 (çalışma durumu temizliği, 2026-11-17)

Kod davranışı değişmedi; sürüm numaraları 0.26.0 olarak kaldı. X-Plane'de denendi
(v0.26 `prime_map.dll` hash doğrulamalı), map_tests 30.682 / 0.

- **`#pragma` kaldırıldı**: 90 başlıkta `#pragma once` → klasik include guard'ları
  (`TerrainVectors.inl` dahil). DLL sınırından geçen STL üyelerinin C4251 uyarısı
  `#pragma` yerine derleyici seçeneği `/wd4251` ile kapatıldı; `StbTruetypeImpl.cpp`
  `/W0` ile izole edildi. Depoda `#pragma` kalmadı.
- **Yorum inceltme**: kütüphane başlık ve kaynaklarında sadeleştirme ("neden"
  notları korundu); dört ağaç /W4 uyarısız.
- **Doküman taraması**: genel belgelerdeki dış depo bağlantıları Bitbucket'e
  çevrildi; API kılavuzu ve mimari belgesi 0.26 içeriğiyle (HTML + PDF) yeniden
  üretildi. SRS ve README'de artık repoda bulunmayan dosyalara atıf yok.
- Önbellek klasör paylaşımı araştırıldı (kod değişikliği yok): paylaşım birimi
  yöneticidir; disk karo önbelleği ayarlardan bağımsız, aday DSF listesi
  anahtarlıdır; farklı süreçler `setCacheDirectory` ile aynı havuzu
  paylaşabilir (içerik doğrulamalı okuma).

## 0.26 (Texture çizim yolu, salt-JSON ayarlar, 2026-11-17)

X-Plane'de **denendi** (resmî eklenti + UI test eklentisi, v0.26). map_tests 30.682 / 0.

- **`WindowStyle::Texture`** (varsayılan): harita artık host'un sağladığı
  framebuffer'a (doku) çizilir; X-Plane penceresi gerekmez. `XMap::renderTexture()`,
  `getTextureId()`, `setTextureSize()`, `sceneSignature()`; yeniden çizim kararı
  imza/görünüm değişiminde, hız sınırı `max_fps`. GL durumu (framebuffer, viewport,
  scissor, MVP) `beginOffscreen` tarafından saklanır ve geri konur.
- **`inject*` girdi API'si**: Texture yolunda pencere yoktur; host imleç / düğme /
  tekerlek / tuş olaylarını `injectMouseMove` vb. ile aktarır, `onClick` / `onWheel` /
  `onKey` aynı yürütücüye gider. Klavye odağı isteği `wantsKeyboard()`.
- **Ayarlar salt JSON**: INI okuma ve eski sürüm dosya göçü tamamen kaldırıldı;
  `prime_map.ini` → `prime_map.json`, `map_settings.ini` →
  `maps/<ad>/map_settings.json`, `terrain_colors.ini` →
  `maps/<ad>/terrain_colors.json`. Depo biçimi yalnızca JSON'dur
  (`Settings`: bölüm/anahtar/değer, değerler metin).
- Dokümanlar yeniden yazıldı (kullanım kılavuzu 26 sayfa, mimari 24 sayfa) ve SRS
  güncellendi.
- LEGACY pencere yolları (`Embedded` / `Standalone`) korunur; Texture'a pop-out ve
  LEGACY yolların kaldırılması planlıdır (SRS).

## 0.25 (dosya düzeni, isimlendirme, JSON ayar deposu, 2026-11-16)

X-Plane'de **denenmedi** (derlendi, 30.682 test yeşil).

- **Her sınıf kendi dosyasında**: çok sınıflı kaynak dosyaları bölündü; h+cpp
  çiftleri CMake listelerinde tek satırda.
- **`XMapInstance` → `XMap` birleşmesi**: ara "instance" katmanı kaldırıldı;
  `XMap` hem API yüzeyi hem motordur (katmanlar, çizimler, uçak, palet, dosyalar,
  `XMapWindow`).
- **Map-\* → XMap-\***: kütüphane içindeki bütün sınıf ve dosya adları `XMap`
  önekine çevrildi (`XMapView`, `XMapWindow`, `XMapTheme`, `XMapImageCache`,
  `XMapFormat`, `XMapSymbols`...).
- **Ayar deposu JSON biçimine geçti**: ayar dosyaları `.json` olarak yazılır/okunur
  (INI desteği 0.26'da tamamen kaldırıldı); revizyon sayaçları otomatik kaydı
  tetikler.

## 0.24 (API yeniden yapılanma, 2026-11-13)

Yapısal standardizasyon: yeni API yüzeyi, isimlendirme, yorum temizliği, CMake ekip standardı.
X-Plane'de **denenmedi** (derlendi, 30.682 test yeşil).

- **Yeni API**: `prime::XMapManagerFactory::createXMapManager()` her çağrıda YENİ yönetici verir
  (singleton yok); yönetici haritaların sahibidir (`addMap` / `removeMap`, `findMap`, `getMaps`,
  `getOpenMapCount`). `XMap`: `initialize` / `close` / `isInitialized` / `onRender` /
  `isDataLoaded` / `setName` / `getColor` + `setColor(MapLayer, Rgb)` / `clearColor` /
  `clearAllColors`. Uygulama `src/api/xmap.cpp` + `src/api/xmapmanager.cpp`
  (özel durum `XMapManagerPrivate.h`).
- **Veri klasörü varsayılanı**: setter boş bırakılırsa `%LOCALAPPDATA%\prime\prime-xmap`
  kullanılır (0.23'te host eklenti klasörüydü). Resmî eklenti artık veri klasörü vermez.
- **Katman rengi API'si**: `XMap::setColor(MapLayer, Rgb)` etkin temanın üzerine yazar;
  mod değişince yeniden uygulanır, `map_settings.ini`'de `[layer_colors]` bölümünde kalıcıdır
  (`layer_key = r g b`). `MapTheme`'de doğrudan rengi olan katmanlar eşlenir
  (`themeLayerColor`); diğer katmanlar reddedilir.
- **İsimlendirme**: sınıflarda `X` öneki (`XMap`, `XMapManager`, `XMapInstance`, `XMapData`,
  `XMapWindow`...); fonksiyonlar lowerCamel (`status()`, `stop()`, `parentOf()`...);
  `OpenConfig` → `MapConfig`. Dosya adları değişmedi.
- **Katman hiyerarşisi** serbest fonksiyonlardan `LayerRules` ad alanına taşındı
  (`parentOf`, `childrenOf`, `topLevelLayers`, `kindOf`, `layerName`, `layerKey`,
  `layerFromKey`, `isLayerDefaultVisible`); yönetici metotları ona delege eder.
- **CMake ekip standardı**: yorum yok, `${PROJECT_NAME}`, yardımcı fonksiyon yok,
  `project(prime-map-api VERSION 0.24.0)`, hedef `prime-map-api` + `OUTPUT_NAME prime_map`;
  h+cpp çiftleri tek satırda. Tüketiciler `prime-map-api` hedefine linkler.
- **Yorum temizliği**: kütüphane + eklenti + test kaynaklarında agresif sadeleştirme
  (gürültü ve banner yorumlar silindi; "neden" notları korundu).
- `XMapWindow::render()` genel sarmalayıcı eklendi (`onDraw` private kaldı);
  `XMap::isDataLoaded` `MapInstance`'ın hazır durumunu `std::atomic<bool>` üzerinden yazar.
- Sürümler 0.24.0'a yükseltildi (PrimeMap.h kVersion*, CMake, eklenti kVersion).

## 0.23 (harita fabrikası, bağımsız çoklu harita, 2026-11-12)

Eski kod değişmeden derlenir (`CreateMap(OpenConfig)`, `Map::Open`, `Export/ImportSettings`).
X-Plane'de **denenmedi** (derlendi, 30.682 test, önizleme).

- **Fabrika**: `MapLibrary::CreateMap()` pencere açmadan harita nesnesi verir; bütün ayarlar
  nesne üzerinden `Open()`'dan önce de sonra da verilir. `Open()` / `Close()` / `IsOpen()`;
  `Close` nesneyi ve ayarlarını korur.
- **Bağımsız haritalar**: her haritanın kendi katmanları, çizimleri, uçak / rota kaynağı, uçuş
  izi, zemin ayarları, durum metni var (yeni `engine/MapInstance`). Ortak: veri ve önbellek.
- **Harita başına dosyalar** (kütüphane yazar): `<veri>/maps/<ad ya da map_<kimlik>>/`
  `map_settings.ini`, `map_drawings.txt`, `terrain_colors.ini`. `SetName`, `GetSettingsDirectory`,
  `SetPersistence(Off | SaveOnly | LoadAndSave)`, `SaveSettings`.
- **Merkezi ayarlar** `prime_map.ini` (`LibrarySettings`, `MapLibrary::GetSettings/SetSettings`):
  ortak önbelleğin disk sınırı ve doku bütçesi açık harita sayısıyla büyür
  (`growth_percent_per_map`, tavanlar), önceden yükleme, otomatik kayıt.
  `InitConfig::mDataDirectory`; `Status`'ta `mOpenMaps`, `mTerrainCacheLimitMb`, `mTextureLimitMb`.
- **Ortak zemin merkezi** (`layers/TerrainHub`): dokular ve vektörler tek kopya, bütün haritalar
  paylaşır; istek önceliği isteyen haritanın merkezine göre.
- **Zemin ayarları harita başına ve anında**: `SetTerrainSettings` artık X-Plane'i yeniden
  başlatmadan uygulanır (palet numarası zemin anahtarında).
- Yeni: `SetZoomLimits` (harita başına 3…18), `SetLayerZoomRange` (katman / alt katman / grup),
  `GetZoomStep`, `GetPanStep`, `SetStyle`, `SetTitle`, `SetImageCache`, `SetMaxRenderFps`,
  `SetXPlaneFont`, `GetStatusText`, `GetTerrainPendingImages`, `GetMaps`, `FindMap`,
  `GetOpenMapCount`, klasör sorguları.
- Düzeltme: yazı çizici süreçte tek işaretçiydi, son açılan pencere alıyordu (çoklu haritada
  yazılar yanlış pencereye gidebiliyordu); her pencere çizerken kendisininkini seçer.
- Zemin: dokusu henüz gelmemiş karo Scenery'de siyah yerine ortalama kara tonu; işçinin aşama
  süreleri Diagnostics modunda loglanır (`TerrainProfile`). Araştırma: `docs/ZEMIN_HIZ_RAPORU.md`.
- Davranış farkı: `[cache]` anahtarları artık harita ayarında değil `prime_map.ini`'de.

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
