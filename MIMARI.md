# Map Plugin — Kod Yapısı

Bu dosya kodun **nerede ne yaptığını** gruplar hâlinde anlatır. Kullanım ve
test listesi `README.md`'de, kararların geçmişi `CLAUDE.md` ve `GOREV.md`'de.
Sürüm: 0.13 (16.600 satır C++, `src/` + `tools/` + `tests/`).

## 1. Büyük resim

```
                X-Plane 11 (XPLM SDK)
                       │  dataref, nav verisi, FMS, pencere, komut
                       ▼
 ┌──────────────── src/Plugin.cpp ────────────────┐
 │ giriş noktaları, menü, komutlar, flight loop   │
 │ (her karede veri depolarına Update/Poll)       │
 └───────┬───────────────────────────┬────────────┘
         │                           │
         ▼                           ▼
 Veri katmanı (src/data)       MapWindow (pencere + fare + UI)
 apt.dat, DSF, nav, hava       │  her çizimde DrawContext kurar
 yolu, uçak — arka plan        ▼
 thread'leriyle hazırlanır  LayerManager → Layer'lar (src/layers)
         │                     │  Mesh üretir, Canvas'a verir
         └──── const & ───────►▼
                           gfx/Canvas (TEK OpenGL kapısı, GL 3.3)
```

Temel kural: **çekirdek kod X-Plane'e bağımlı değil**. XPLM çağrısı sadece
`Plugin.cpp`, `MapWindow.cpp`, `data/NavLoader.cpp`, `data/AircraftData.cpp`,
`layers/FmsLayer.cpp`, `Log.cpp`, `gfx/Canvas.cpp`, `gfx/Texture.cpp` ve
`ui/UiDraw.cpp` (yazı) içinde. Geri kalan her
şey testlerde (`tests/`) ve önizleme aracında (`tools/preview`) X-Plane'siz
çalışır.

## 2. Bir karede ne oluyor?

1. **Flight loop** (`Plugin.cpp`, her kare): nav verisi 4 ms'lik dilimlerle
   yüklenir (`NavLoader`); arka plan işlerinin bitip bitmediğine bakılır
   (`AirportGeometryStore::Poll`, `AirwayDatabase`, `TerrainStore`);
   `AirportDetailStore::Update` istenen havalimanlarını işçi thread'e verir;
   uçak konumu okunur (`AircraftData`); iz örneklenir; arka plan thread'lerinin
   rapor metinleri Log'a yazılır.
2. **Pencere çizimi** (`MapWindow::Draw`): arka plan teması boyanır,
   `DrawContext` kurulur (görünüm `MapView`, `Canvas`, `LabelPlacer`, tema
   `MapTheme`), `LayerManager` katmanları alttan üste `Draw` eder, sonra
   etiketleri öncelik sırasıyla `DrawLabels` ile yerleştirir. En üstte araç
   şeridi, paneller, durum çubuğu ve fare açıklaması (tooltip).
3. **Fare** (`MapWindow`): 4 px'ten az hareket = tıklama, fazlası = sürükleme
   (kaydırma); tekerlek imleç etrafında zoom; çizim modunda tıklama nokta/rota
   ekler, sağ tık bitirir.

## 3. Gruplar

### 3.1 Giriş ve pencere — `src/`
| Dosya | Görev |
|---|---|
| `Plugin.cpp` | `XPluginStart/Stop/Enable/Disable`; menü "Map Plugin"; komutlar `ozdev/map/toggle`, `controls`, `diagnostics`, `next_mode`; flight loop; `CreateLayers()` = katman sırası |
| `MapWindow.*` | `XPLMCreateWindowEx` penceresi (pop-out destekli), fare/tekerlek, araç şeridi olayları, harita modu değişimi, ayarların yazılması, tooltip |
| `Settings.*` | Basit INI okuyucu/yazıcı (`map_settings.ini`) |
| `Log.*` | `Log("x = {}", t)` → X-Plane Log.txt; sadece ana thread |
| `BackgroundJob.*` | Tek seferlik arka plan işi (thread + `Poll`) |

### 3.2 Harita çekirdeği — `src/`
| Dosya | Görev |
|---|---|
| `MapView.*` | "Kamera": Web Mercator, merkez + sürekli zoom, lat/lon ↔ boxel dönüşümü |
| `ScreenMath.*` | Ekran koordinatı yardımcıları (lat/lon, boxel, piksel ayrımı) |
| `Geo.*` | Büyük daire mesafe/ara nokta, poligon alanı/üçgenleme (çizimler için) |
| `GeoGrid.*` | 1×1 derece uzamsal indeks: sadece ekrandaki hücrelere bakılır |
| `MapFormat.*` | Koordinat/mesafe yazı biçimleri, ızgara ve ölçek çubuğu aralığı |
| `LabelPlacer.*` | Etiket/sembol çakışma önleme (ilk isteyen kazanır, 32 boxel hücreler) |
| `Layer.h` | Katman arayüzü: `Draw`, `DrawLabels`, `Describe` (tooltip), `LabelPriority`; `DrawContext` |
| `LayerManager.*` | Katman listesi, aç/kapa, mod başına görünürlüğün kaydı (`[layers.<mod>]`) |
| `MapTheme.*` | Harita modları (Scenery / VFR / IFR Low / IFR High): renkler, zemin biçimi, varsayılan katmanlar |
| `Color.h` | `Rgba` |
| `Drawings.*` | Kullanıcı çizimlerinin modeli (nokta, rota, bölge), `map_drawings.txt` |

### 3.3 Veri — `src/data/`
Hepsi arka planda hazırlanır; ana thread sonucu `Poll`/`Update` ile devralır.
Arka plan thread'inde XPLM ve Log **yok**, rapor metni döner.

**Havalimanları (apt.dat, kendi okuyucumuz; XP-APT-1100 spesifikasyonu)**
| Dosya | Görev |
|---|---|
| `AptLine.*` | Tek satırı yerel ayardan bağımsız okuyan alan okuyucu |
| `AptDatIndexer.*` | Açılışta tüm apt.dat'ları bir kez tarar → özet + dosyadaki bayt aralığı; disk önbelleği `cache/airports/airport_index.bin` |
| `AirportGeometry.*` | Özet yapısı (kimlik, ad, tür, pistler, kapsayan kutu, kule) |
| `AirportGeometryStore.*` | Özetlerin deposu + 1×1 ızgara; pist ve havalimanı katmanları buradan okur |
| `AirportDetail.h` | Ayrıntı yapısı: apt.dat satır kodlarının birebir karşılığı |
| `AptDatParser.*` | Tek havalimanının metnini `AirportDetail`'e çevirir |
| `AirportShapes.*` | Bezier eğrilerini düzleştirir, delikli yüzeyleri earcut ile üçgenler, ışık dizilerini üretir |
| `AirportDetailStore.*` | Ekrandaki havalimanlarının ayrıntısını işçi thread'de okur/üçgenler; LRU 64 |
| `LocalizerMatch.*` | ILS'nin hizmet ettiği pisti bulur (tüy eşikten başlasın diye) |

**Nav ve hava yolları**
| Dosya | Görev |
|---|---|
| `NavDatabase.*` | VOR/DME/NDB/ILS/fix/havalimanı kayıtları + tür başına ızgara |
| `NavLoader.*` | SDK'dan (`XPLMGetFirstNavAid` ...) dilimler hâlinde doldurur |
| `AirwayDatabase.*` | `earth_awy.dat` (1100); Custom Data önce |

**Zemin (kurulu DSF sahnesinden)**
| Dosya | Görev |
|---|---|
| `SceneryPacks.*` | `scenery_packs.ini` sırası (sadece okunur) |
| `SceneryIndex.*` | Her 1×1 karo için aday DSF'ler, X-Plane'in öncelik kuralıyla |
| `SevenZip.*` | 7z sıkıştırılmış DSF'leri açar (`third_party/lzma`) |
| `DsfReader.*` | DSF: arazi türü adları, üçgenler, DEM rakımı |
| `TerrainClasses.*` | Arazi türü adı → sınıf (su, şehir, orman ...), palet, `ElevationColor` rampası |
| `TerrainRaster.*` | Karo başına sınıf (4096²), gölge (1024²), rakım (512²) raster'ı; RLE disk önbelleği (sürüm 2) |
| `TerrainStore.*` | Zemin işçi thread'i: istenen karo + LOD + biçim (mod) için RGBA görüntü üretir |

**Uçak**
| `AircraftData.*` | Kullanıcı uçağının konumu/yönü (dataref) |
|---|---|

### 3.4 Katmanlar — `src/layers/`
Çizim sırası (alttan üste, `Plugin.cpp::CreateLayers`):

| Katman | Veri kaynağı | Not |
|---|---|---|
| `TerrainLayer` | TerrainStore | Doku LRU 256 MB, karede en çok 3 yükleme |
| `AirportSurfaceLayer` | AirportDetailStore | Yüzeyler bir kez `StaticMesh`'e (GPU) yüklenir |
| `GridLayer` | — | Zoom'a göre aralık |
| `NavaidLayer` (ILS) | NavDatabase + pistler | Tüy pistlerin altında |
| `RunwayLayer` | AirportGeometryStore | Gerçek ölçekli pistler, numaralar |
| `TaxiRouteLayer` | AirportDetailStore | 1201/1202, varsayılan kapalı |
| `AirportObjectsLayer` | AirportDetailStore | Kapı/park, helipad, beacon, tulum, PAPI, ışık, tabela |
| `AirwayLayer` ×2 | AirwayDatabase | Alçak / yüksek |
| `AirportLayer` | NavDatabase (+ kule bilgisi özetten) | Havalimanı sembolleri ve adları |
| `NavaidLayer` (VOR, NDB, Fix) | NavDatabase | VFR'de VOR pusula gülü |
| `FmsLayer` | XPLM FMS | Uçuş planı |
| `TrackLayer` | AircraftData | Uçuş izi |
| `AircraftLayer` | AircraftData | Uçak sembolü |
| `DrawingLayer` | Drawings | Kullanıcı çizimleri |

Yardımcılar: `MapSymbols.*` (ortak ölçüler, etiket yerleştirme),
`AirportVisibility.*` (ekrandaki havalimanlarını merkeze yakınlık sırasıyla bulur).

Yeni katman eklemek: `Layer`'dan türet → `Draw`/`DrawLabels`/`Describe` yaz →
`CreateLayers()`'ta doğru sıraya ekle → `MapTheme` varsayılan görünürlüğü →
`ui/LayerPanel.cpp` `kGroups` tablosunda grubuna koy.

### 3.5 Çizim — `src/gfx/` (OpenGL sadece burada)
| Dosya | Görev |
|---|---|
| `Mesh.*` | CPU'da renkli üçgen listesi; kalın çizgi, daire, çerçeve hepsi üçgen. `OutlinedLines`: önce kontur (hale), sonra renkli çizgi. Testli |
| `Canvas.*` | Tek çizim kapısı: VBO + VAO + GLSL 330; X-Plane'in GL durumunu kaydedip geri koyar |
| `StaticMesh.*` | GPU'da kalıcı şekil, her karede ölçek + kaydırma ile çizilir |
| `Texture.*` | RAII doku (`XPLMGenerateTextureNumbers`, `XPLMBindTexture2d`) |
| `GlApi.*` | GL 3.3 fonksiyonlarını `wglGetProcAddress` ile yükler |
| `GlScreen.*` | boxel → piksel dönüşümü (UI ölçeği), scissor |

### 3.6 Arayüz — `src/ui/`
| Dosya | Görev |
|---|---|
| `Toolbar.*` | Üst şerit (dar pencerede iki satır): Layers, Map: <mod>, takip, çizim araçları, Pop out |
| `LayerPanel.*` | Gruplu katman listesi (üç durumlu kutu, aç/kapa oku) |
| `ModePanel.*` | Harita modu seçimi |
| `StatusBar.*` | İmleç koordinatı, zoom, mesaj, ölçek çubuğu |
| `UiDraw.*` | Kutu, düğme, onay kutusu, gölgeli yazı (`XPLMDrawString`, sadece ASCII); `Ui::k...` paleti |

### 3.7 Araçlar ve testler
| Yer | Görev |
|---|---|
| `tests/tests.cpp` | X-Plane'siz çekirdek testleri (355 kontrol): Mesh, MapView, LabelPlacer, apt.dat ayrıştırma, Bezier, DSF, ayarlar, temalar ... |
| `tools/preview/` | `map_preview`: aynı katman kodunu XPLM taklitleriyle (`XplmStubs`) PPM resme çizer; nav verisini dosyadan okur (`FileNavLoader`). `--lat --lon --zoom --mode --show --hide --panel --describe` |
| `tools/dsfinfo/` | `dsf_info`: DSF dosyasının içeriğini döker (biçim incelemesi) |
| `third_party/` | XPSDK 3.0.1, LZMA SDK 23.01, mapbox earcut 2.2.4 (tek belgelenmiş hız yaması, `README.txt`) |
| `legacy/` | İlk prototip (eski, derlenmiyor; sadece başvuru) |

## 4. Thread'ler

| Thread | Ne yapar |
|---|---|
| Ana (X-Plane) | Tüm XPLM çağrıları, Log, çizim, NavLoader dilimleri |
| AirportGeometryStore işi | apt.dat indeksleme (bir kez, açılışta) |
| AirportDetailStore işçisi | Ekrandaki havalimanlarının ayrıştırma + üçgenleme |
| AirwayDatabase işi | earth_awy.dat okuma (bir kez) |
| TerrainStore işçisi | DSF açma, raster üretme, önbellek, mod biçiminde görüntü |

Sonuçlar kilitli kuyruklar veya `BackgroundJob::Poll` sonrası ana thread'de
devralınır; katmanlar sadece ana thread'de çalışır.

## 5. Diskteki dosyalar (eklenti klasörü)

`C:\X-Plane 11\Resources\plugins\MapPlugin\`
- `64/win.xpl` — eklenti
- `map_settings.ini` — pencere, görünüm, mod, mod başına katmanlar
- `map_drawings.txt` — kullanıcı çizimleri
- `terrain_colors.ini` — zemin paleti (kullanıcı değiştirebilir)
- `cache/terrain/*.terrain`, `cache/airports/airport_index.bin` — silinebilir önbellekler
