---
title: Prime XMap
subtitle: Mimari
version: 1.1.1
date: 2026-11-19
---

# Amaç

Prime XMap, X-Plane 11 eklentilerine gömülen katmanlı bir harita kütüphanesidir. Harita, yalnızca o bilgisayarda kurulu X-Plane verisini kullanır:

- Zemin: kurulu DSF sahne dosyaları (arazi türü, su, kabartma, yollar).
- Havalimanları: `apt.dat` dosyaları (pistler, yüzeyler, boya çizgileri, park yerleri, ışıklar).
- Navaid'ler: X-Plane SDK'sının nav veritabanı.
- Hava yolları: `earth_awy.dat`.
- Uçak ve rota: dataref'ler ve FMS.

Böylece haritada görünen her şey simülatördekiyle aynıdır ve yeni bir sahne kurulduğunda harita kendiliğinden güncellenir.

X-Plane'in kendi haritasına (`XPLMMap`) katman eklemek yerine ayrı bir harita çizilmesinin nedeni, o haritanın yan panelinin kaldırılamaması, kendi katmanlarının tek tek kapatılamaması ve eklenen katmanların fare olayı alamamasıdır.

Bu belge kodun nasıl düzenlendiğini ve parçaların nasıl birlikte çalıştığını anlatır. Kullanım ve API başvurusu için `PrimeMap_API` belgesine, gereksinimler için `SRS.md` belgesine bakın.

# Büyük resim

```text
Host eklenti
   │  PrimeMap.h
   ▼
XMapManagerFactory ──► XMapManager ─────────────── sahibi ───► XMap, XMap, ...
                          │                                       │
                          ▼                                       ▼
                       XMapData (ortak)                       XMapWindow
                       ├─ NavDatabase + NavLoader             ├─ XMapView (kamera)
                       ├─ AirportGeometryStore                ├─ LayerManager + katmanlar
                       ├─ AirportDetailStore (işçi)           ├─ arayüz (ui/)
                       ├─ AirwayDatabase                      └─ Gfx::Canvas (OpenGL)
                       ├─ TerrainStore (işçi) + TerrainHub
                       └─ flight loop                         Drawings, AircraftData,
                                                              TerrainPalette, ayar dosyaları
```

| Klasör | İçerik |
|---|---|
| `PrimeMap.h`, `primemapglobal.h` | Host'un dahil ettiği başlık ve DLL dışa aktarım makrosu |
| `src/api/` | Genel API: `XMapManagerFactory`, `XMapManager`, `XMap`, `XMapTypes` |
| `src/engine/` | `XMapData` (bir yöneticinin ortak verisi) ve `LayerTable` (katman kimlikleri, ağaç kuralları, zoom eşikleri) |
| `src/` | Çekirdek: `XMapWindow`, `XMapView`, `Layer`, `LayerManager`, `XMapTheme`, `Settings`, `Drawings`, `LabelPlacer`, `Geo`, `GeoGrid`, `Log` |
| `src/data/` | Veri okuyucular: `apt.dat`, `earth_awy.dat`, nav veritabanı, DSF ve 7z, zemin üretimi, önbellek |
| `src/layers/` | Her harita katmanının çizim kodu |
| `src/gfx/` | OpenGL 3.3 çizim katmanı; projede OpenGL çağrısı yalnızca burada yapılır |
| `src/ui/` | Haritanın kendi arayüzü: araç şeridi, katman ve mod panelleri, çizim paleti, arama, durum şeridi |
| `tests/` | X-Plane olmadan çalışan birim testleri |
| `tools/` | `map_preview` (katmanları resme çizer) ve `dsf_info` (DSF dökümü) |
| `examples/texture_host/` | En küçük tam host eklenti örneği |

Kodu ilk kez okuyan biri için önerilen sıra: `api/XMapManager.cpp` → `engine/XMapData.cpp` → `api/XMap.cpp` → `XMapWindow.cpp` → `Layer.h` → basit bir katman (`layers/GridLayer.cpp`) → `gfx/Mesh.h` ve `gfx/Canvas.h` → ilgilenilen veri deposu.

# Çekirdek ve X-Plane kabuğu

Kodun büyük kısmı X-Plane'e bağımlı değildir. XPLM fonksiyonları yalnızca ince bir kabukta çağrılır; bu sayede çekirdek X-Plane açılmadan testlerde ve önizleme aracında çalışır.

| Kabuk dosyaları | XPLM kullanım nedeni |
|---|---|
| `api/*`, `engine/XMapData.cpp` | Yol sorguları, flight loop kaydı |
| `XMapWindow.cpp` | Eski stillerde pencere, fare ve klavye callback'leri |
| `data/NavLoader.cpp` | Nav veritabanını gezmek |
| `data/AircraftData.cpp` | Uçak dataref'leri |
| `layers/FmsLayer.cpp` | FMS |
| `Log.cpp` | `Log.txt` |
| `gfx/Canvas.cpp`, `gfx/Texture.cpp` | Grafik durumu ve doku numaraları |

CMake bu ayrımı iki listeyle yapar: `CORE_SOURCES` kütüphaneye, testlere ve önizleme aracına girer; `PLUGIN_SOURCES` yalnızca kütüphaneye girer. Önizleme aracı XPLM fonksiyonlarının yerine `tools/preview/XplmStubs` taklitlerini kullanır.

# API katmanı

## Fabrika

`XMapManagerFactory::createXMapManager()` her çağrıda yeni bir `XMapManager` döner; tekil nesne yoktur. Yöneticinin kurucusu gizlidir, böylece nesne DLL'in belleğinde oluşturulup yine orada silinir.

Fabrika fonksiyonu başlıkta satır içi tanımlıdır. Bu sayede host'un derlendiği sürüm sabitleri host'un ikili dosyasına işlenir ve fabrika bunları yüklü DLL'in `versionString()` değeriyle karşılaştırır. Sürümler farklıysa `Log.txt`'ye `host built with X, loaded DLL is Y at <yol>` uyarısı yazılır. `libraryPath()` yüklü DLL'in yolunu `GetModuleHandleExW` ve `GetModuleFileNameW` ile bulur.

## Yönetici

`XMapManager` klasörleri ve log ayarlarını tutar, `initialize()` ile `XMapData`'yı oluşturur ve haritaların sahibidir.

- Veri klasörü verilmezse `%LOCALAPPDATA%\prime\prime-xmap\`, önbellek klasörü verilmezse `<veri>/cache/` kullanılır.
- `initialize()` süreç içindeki canlı yöneticilerin kaydına bakar. Aynı veri klasörünü kullanan ikinci bir yönetici uyarı yazar; yalnızca önbellek klasörünü paylaşan yönetici bilgi satırı yazar.
- `addXMap()` haritayı oluşturur ama açmaz. `XMapConfig` değerleri haritaya uygulanır.
- `shutdown()` açık haritaları kapatır, `XMapData`'yı durdurur. Elde kalan harita işaretçileri bundan sonra etkisizdir; her genel metot `alive()` denetimiyle başlar.

## Harita

`XMap` tek bir haritanın her şeyidir. Kurucusu katman nesnelerini, `XMapWindow`'u, çizim modelini, uçak verisini ve zemin paletini oluşturur. Bu yüzden harita açılmadan önce de bütün ayar çağrıları çalışır.

`XMap::initialize()` sırası:

1. `LoadAndSave` kalıcılığında ayar dosyaları okunur ve o ana kadar kodla verilen değerlerin üzerine uygulanır.
2. `XMapWindow::create()` doku hedefini (Texture) ya da pencereyi (eski stiller) hazırlar.
3. `XMapData` açık harita sayısını yeniden hesaplar ve önbellek sınırlarını günceller.
4. Kalıcılık açıksa ayar klasörü ve dosyalar yazılır.

Harita her karede `XMapData`'nın flight loop'undan `update()` alır: uçak verisi yenilenir, katmanlar güncellenir, durum metni kurulur ve ayar ya da çizim revizyonu değiştiyse dosyalar otomatik olarak (en sık iki saniyede bir) yazılır.

# Ortak veri ve flight loop

`XMapData` bir yöneticinin bütün haritaları arasında paylaşılan veriyi tutar: nav veritabanı, havalimanı özetleri ve ayrıntı deposu, hava yolları, zemin işçisi ve `TerrainHub`, merkezi ayarlar ve önbellek sınırları.

Her karede yapılanlar:

```text
XMapData::onFlightLoop
    stepLoading         yükleme adımları (aşağıda)
    hub.beginFrame      karede yüklenebilecek doku sayısını sıfırlar
    açık harita yoksa   uçak dataref'lerini okur
    updatePrefetch      uçağın çevresi, uçuş yönü ve rota için zemin karoları ister
    updateStatus        ortak durum metni
    XMap::update        her harita için
```

Yükleme adımları:

1. İlk karede havalimanı indeksleme işi başlar ve zemin işçisi kurulu sahnenin dizinini çıkarmaya başlar. `initialize()` bunları hemen başlatmaz; eklenti etkinleşirken simülatörün verisi henüz hazır olmayabilir ve X-Plane'in açılışı bekletilmemelidir.
2. İşçilerin log satırları ana thread'e aktarılır.
3. Havalimanı indeksi bitince ayrıntı deposu başlar.
4. Nav verisi her karede en fazla 4 ms kullanılarak okunur; SDK yalnızca ana thread'den çağrılabilir.
5. Nav verisi bitince hava yolları arka planda yüklenir.

Önceden yükleme, açık bir harita varsa haritaların uçağına ve rotasına, hiç açık harita yoksa yöneticinin kendi okuduğu uçak konumuna göre çalışır. Uçak 40 knot'tan hızlıysa uçuş yönünde 15 NM aralıklarla 120 NM'ye kadar karolar istenir. `prefetch_tiles` sıfırsa önceden yükleme yapılmaz.

Önbellek sınırları açık harita sayısına göre `applyLimits()` ile hesaplanır: `sınır(n) = taban × (1 + artış% × (n − 1) / 100)`, tavan verilmişse onu aşmaz. Disk sınırı, doku bütçesi, işçinin bellek listeleri ve yüklü havalimanı sayısı bu formülle büyür.

# Thread modeli

```text
Ana thread                       Arka plan thread'leri
──────────────────────────       ──────────────────────────────────────────
XPLM, Log, OpenGL                havalimanı indeksleme (bir kez)
katmanlar, doku yükleme          havalimanı ayrıntı işçisi (sürekli, LRU)
nav okuma dilimleri              hava yolları (bir kez)
                                 zemin işçisi (sürekli, düşük öncelik)
          istek kuyruğu ──► (kilitli) ──► sonuç kuyruğu
```

Kurallar:

- Arka plan thread'leri hiçbir XPLM fonksiyonunu, `Log` dahil, çağırmaz. Rapor metni bırakırlar; ana thread bunu yazar.
- Katmanlar yalnızca ana thread'de çalışır.
- İşçiyle paylaşılan kuyruklar `std::mutex` ve `std::condition_variable` ile korunur. Tek seferlik işler `BackgroundJob::poll()` ile devralınır.
- Katmanlar veriyi istekle alır. Bir havalimanının ayrıntısı hazır değilse katman isteği bırakır ve o karede yer tutucu çizer; yavaş bir iş hiçbir zaman çizimi bekletmez.

# Koordinatlar ve görünüm

| Sistem | Birim | Kullanım |
|---|---|---|
| Coğrafi | Derece | Bütün veri, çizimler ve ayarlar |
| Mercator | 0..1, dünya tek bir kare | `XMapView` merkezi; havalimanı şekilleri bir referansa göre fark olarak |
| Boxel | XPLM ekran birimi | Bütün çizim ve fare koordinatları; UI ölçeği %100 değilse pikselden farklıdır |
| Piksel | Gerçek ekran pikseli | Kırpma (`glScissor`); dönüşüm `gfx/GlScreen` |

`XMapView` haritanın kamerasıdır: Web Mercator, merkez ve sürekli zoom. Zoom 0'da dünya 256 boxel'dir ve her adımda kenar iki katına çıkar. Mercator'da ekrana çevirme bir ölçek ve bir kaydırmadan ibaret olduğu için değişmeyen şekiller GPU'ya bir kez yüklenip her karede yalnızca bu iki değer değiştirilerek çizilebilir.

# Katman sistemi

## Layer arayüzü

```cpp
class Layer
{
    virtual void update(float pNow);
    virtual bool prepareFrame();
    virtual bool isOverlay() const;
    virtual void draw(const drawContext& pContext) = 0;
    virtual void drawLabels(const drawContext& pContext);
    virtual void drawOverlay(const drawContext& pContext);
    virtual std::string describe(const drawContext& pContext, double pX, double pY) const;
    virtual int labelPriority() const;
};
```

`drawContext` çizim sırasında katmanlara görünümü, `Gfx::Canvas` çizim kapısını, ortak etiket yerleştiricisini ve geçerli temayı verir.

- `draw` geometriyi üretir ve `Canvas`'a verir.
- `drawLabels` yazıları çizer; `LabelPlacer`'dan yer ister.
- `describe` imlecin altındaki nesne için kısa açıklama döner; tooltip ve `queryAt` bunu kullanır.
- `labelPriority` etiket yerleşim sırasıdır: havalimanı 10, pist 15, VOR 20, NDB 30, ILS 40, fix 60, hava yolu 70. Önce gelenin etiketi yer bulur; çakışan etiket çizilmez.
- `isOverlay` her karede değişen katmanları (uçak, iz) ayırır; bu katmanlar resim önbelleğinin üstüne çizilir.

Alt katmanlar katmanın kurucusunda `addSubLayer` ile tanıtılır; `draw` içinde `isSubLayerDrawn` ile hem görünürlük hem zoom aralığı denetlenir.

## Zoom modeli

Her katman ve alt katman üç değer taşır: yerleşik görünme aralığı, sert alt sınır ve host'un verdiği aralık. Etkin aralık şöyledir:

```text
host aralığı verildiyse : en küçük = max(host_min, sert_alt_sınır), en büyük = host_max
verilmediyse            : en küçük = max(yerleşik_min, sert_alt_sınır), en büyük = yerleşik_max
```

Yerleşik değerler ve sert sınırlar `engine/LayerTable.cpp` içindeki `kZoomTable` tablosundadır; `LayerRules::applyBuiltInZoomRanges` harita kurulurken bunları katmanlara uygular. Katman kodu görünürlüğe karar verirken yalnızca `isInZoomRange()` ve `isSubLayerDrawn()` kullanır; kod içinde ayrıca bir görünme eşiği yoktur. Etiket, yön oku ve VOR gülü gibi ayrıntıların eşikleri katman kodunda kalır.

Havalimanı sembollerinde kategori eşikleri (büyük 4, küçük 8, deniz üssü 9, heliport ve kapalı 11) yoğunluk ayrıntısıdır. Etkin en küçük zoom yerleşik değerin altına çekildiğinde bütün kategoriler aynı miktarda kayar.

Sert alt sınırlar ölçümle belirlenmiştir. Her katman tek başına ve bütün alt katmanları açık olarak, kurulu verinin en yoğun olduğu iki bölgede (Seattle ve New York, 1200×800 boxel) yarımşar zoom adımıyla çizilmiş ve iki bölgeden kötü olanın süresi alınmıştır. Sert alt sınır, katmanın çizim süresinin 5 ms'yi aşmadığı en küçük zoom'dur; katmanın kendi eşiğindeki süresi zaten 5 ms'nin üzerindeyse o süre ölçüt alınmıştır. Ölçüm `map_preview --bench` ile yapılır.

| Katman | Yerleşik eşik | Sert alt sınır | Süre, yerleşik eşikte (ms) | Süre, sert alt sınırda (ms) |
|---|---|---|---|---|
| Havalimanı sembolleri | 4 | 3 | 11,1 | 10,5 |
| VOR | 5 | 3 | 2,0 | 3,0 |
| NDB | 7 | 3 | 0,4 | 3,2 |
| ILS | 8,5 | 4 | 0,4 | 4,7 |
| Fix | 9 | 7 | 1,8 | 4,3 |
| Alçak hava yolları | 6,5 | 5 | 2,2 | 4,6 |
| Yüksek hava yolları | 6,5 | 6 | 2,9 | 4,4 |
| Pistler | 11 | 4 | 0,3 | 4,0 |
| Havalimanı yüzeyleri | 10,5 | 6 | 0,5 | 3,3 |
| Havalimanı nesneleri | 12,5 | 12 | 1,9 | 4,0 |
| Taksi ağı | 13,5 | 9,5 | 0,3 | 0,9 |

Havalimanı sembollerinin süresi, karede en fazla 3000 sembol çizilmesi sınırı nedeniyle zoom azaldıkça artmaz. Zemin katmanının sert alt sınırı (4) çizim süresine değil, uzak zoomda istenecek karo sayısına göre belirlenmiştir. Yol alt katmanları karede en fazla 16 karo çizdiği için sınırları yerleşik eşiklerin bir altında bırakılmıştır. Görünümün en küçük zoom'u 3 olduğundan 3 değerindeki sert sınır pratikte sınırsızdır.

## Çizim sırası

Katmanlar alttan üste şu sırayla çizilir:

| Sıra | Katman | Veri | Çizdiği |
|---|---|---|---|
| 1 | `TerrainLayer` | `TerrainStore`, `TerrainHub` | Zemin dokuları |
| 2 | `RoadLayer` | `TerrainStore` vektörleri | Yollar, demiryolları, enerji hatları |
| 3 | `AirportSurfaceLayer` | `AirportDetailStore` | Apron ve taksi yolu yüzeyleri, kenarlar, sınır, boya çizgileri |
| 4 | `GridLayer` | — | Enlem ve boylam ızgarası |
| 5 | `NavaidLayer` (ILS) | `NavDatabase`, pistler | Localizer yaklaşma şekli; pistlerin altında kalsın diye burada |
| 6 | `RunwayLayer` | `AirportGeometryStore` | Gerçek ölçekli pistler, uzantılar, işaretler, numaralar |
| 7 | `TaxiRouteLayer` | `AirportDetailStore` | Taksi ağı ve adları |
| 8 | `AirportObjectsLayer` | `AirportDetailStore` | Işıklar, helipad, beacon, rüzgâr tulumu, park yerleri, PAPI, tabelalar |
| 9–10 | `AirwayLayer` (alçak, yüksek) | `AirwayDatabase` | Hava yolları, adları, yön okları |
| 11 | `AirportLayer` | `NavDatabase`, özetler | Havalimanı sembolleri ve adları |
| 12–14 | `NavaidLayer` (VOR, NDB, fix) | `NavDatabase` | Semboller, kimlik ve frekans, VFR'de VOR gülü |
| 15 | `FmsLayer` | FMS ya da host | Uçuş planı |
| 16 | `TrackLayer` | `AircraftData` | Uçuş izi |
| 17 | `AircraftLayer` | `AircraftData` | Uçak sembolü |
| 18 | `DrawingLayer` | `Drawings` | Kullanıcı çizimleri |

Ortak yardımcılar: `layers/XMapSymbols` sembol ölçüleri ve etiket yerleştirme; `layers/AirportVisibility` ekrana değen havalimanlarını merkeze yakınlık sırasıyla bulur, böylece ortadaki havalimanının ayrıntısı önce okunur.

## Yeni katman eklemek

1. `Layer`'dan türeyen bir sınıf yazılır (örnek `layers/GridLayer`).
2. Renkler `drawContext::mTheme`'den okunur; gerekirse `XMapTheme`'e alan eklenir.
3. Katman `XMap::createLayers()` içinde doğru sıraya eklenir ve CMake listesine konur.
4. `XMapLayer` numaralandırmasına ve `LayerTable` tablosuna kimliği, gerekiyorsa `kZoomTable`'a görünme eşiği eklenir.
5. Mod başına varsayılan görünürlük `XMapTheme.cpp` tablolarına yazılır.

Panelde görünmesi, ayar dosyasına kaydı ve alt katman listesi kendiliğinden gelir.

# Havalimanı verisi

`apt.dat` okuyucu projeye aittir ve XP-APT-1100 spesifikasyonuna göre yazılmıştır. Global Airports `apt.dat` dosyası yüzlerce MB olduğu için okuma iki seviyelidir.

**Özet** açılışta bir kez çıkarılır. `AptDatIndexer` bütün `apt.dat` dosyalarını 4 MB'lık parçalarla tarar; her havalimanı için başlık, pistler, kapsadığı alan, kule ve dosyadaki bayt aralığı kaydedilir. Sonuç `cache/airports/airport_index.bin` dosyasına yazılır; anahtar her `apt.dat` dosyasının yolu, boyutu ve değişiklik zamanıdır. `AirportGeometryStore` özetleri 1×1 derecelik ızgarada tutar.

**Ayrıntı** yalnızca ekrandaki havalimanları için okunur. Katman `AirportDetailStore::use(i)` çağırır; ayrıntı hazırsa döner, değilse istek kaydedilir. `update()` her karede istekleri işçiye verir. İşçi dosyada ilgili bayt aralığına gider, `AptDatParser` satırları `AirportDetail` yapısına çevirir, `AirportShapes` Bezier eğrilerini düzleştirir, yüzeyleri earcut ile üçgenler ve çizgi ile ışık dizilerini üretir. Ayrıntılar LRU listesinde tutulur.

| Konu | Kural |
|---|---|
| Öncelik | Custom Scenery paketleri `scenery_packs.ini` sırasıyla, ardından varsayılan `apt.dat`. Aynı kimlikte ilk gelen kazanır. `scenery_packs.ini` yalnızca okunur. |
| Sayı okuma | `AptLine` sayıları yerel ayardan bağımsız okur. |
| Hesap düzlemi | `AirportShapes` hesapları havalimanı çevresinde metre cinsinden düz bir düzlemde yapar; sonuç noktaları bir referansa göre Mercator farkı olarak saklanır. |
| Gerçek veri | Gerçek dosyalar spesifikasyondan bazı noktalarda ayrılır: düğüm satırlarının sonunda 0–3 stil değeri bulunabilir, başlıktaki kule alanı her zaman 0'dır, Bezier kontrol noktası düğümden çıkan eğriye aittir. Biçimle ilgili bir değişiklikten önce gerçek dosyaya bakılmalıdır. |
| ILS | SDK localizer'ın yalnızca anten konumunu verir. `LocalizerMatch` yaklaşma şeklinin pist eşiğinden başlaması için uyumlu pist ucunu bulur. |

`AirportSurfaceLayer` bir havalimanının şekillerini ilk görüldüğünde bir kez `Gfx::StaticMesh` olarak GPU'ya yükler; sonraki karelerde yalnızca ölçek ve kaydırma değişir.

# Zemin

Zemin o bilgisayarda kurulu her sahneyle çalışır. Özel sahneler global sahneyi ezer; sahne güncellenince harita da güncellenir. Bütün iş zemin işçisinde yapılır.

```text
SceneryPacks + SceneryIndex     her 1x1 derecelik karo için aday DSF dosyaları
        │
SevenZip → DsfReader            7z açma; arazi türleri, üçgenler, yükseklik verisi
        │
TerrainClasses                  arazi türü adı → çizim sınıfı
        │
TerrainRaster / TerrainElevation / TerrainVectors    disk önbellekli karo verisi
        │
görüntü üretimi                 seviye, biçim ve palete göre RGBA doku
        │
TerrainHub → TerrainLayer       GPU'ya yükleme ve çizim
```

## Aday dosyalar ve öncelik

`SceneryIndex`, X-Plane'in kendi sırasını taklit eder: Custom Scenery paketleri `scenery_packs.ini` sırasıyla, ardından Global Scenery. Overlay olmayan ilk DSF karonun zeminidir. DSF'i olmayan karo için `Resources/dsf_files` dünya tablosu kullanılır: tabloda olmayan karo denizdir; tabloda olup bu bilgisayarda sahnesi olmayan karo "veri yok" olarak nötr renkle boyanır.

## Seviyeler

| Seviye | Ne zaman | Görüntü | Kaynak |
|---|---|---|---|
| 0 | Çok uzak | 64 px, bütün karo | Sınıf rasteri |
| 1 | Uzak | 256 px, bütün karo | Sınıf rasteri |
| 2 | Orta | 1024 px, bütün karo | Sınıf rasteri |
| 3 | Yakın | 4×4 parçadan biri, 1024 px | Sınıf rasteri |
| 4 | Daha yakın | 1/32 derecelik parça, 1024 px | DSF üçgenleri |
| 5 | Çok yakın | 1/128 derecelik parça, 1024 px | DSF üçgenleri |

Seviye ekrandaki derece başına piksel sayısına göre seçilir. İnce seviye hazır olana kadar eldeki en iyi kaba seviye gösterilir.

## Disk önbelleği

| Dosya | İçerik |
|---|---|
| `<karo>.terrain` | 4096² sınıf rasteri ve alan katmanı |
| `<karo>.elevation` | 1201² yükseklik verisi ve deniz tabanı; fark kodlaması ve LZMA ile sıkıştırılmış |
| `<karo>.vectors` | Orman ve yerleşim alanları, yollar, dışlama alanları |
| `<karo>.nobase` | Yalnızca overlay DSF'i olan karo işareti |

Her dosya, karonun bütün aday DSF dosyalarının yol, boyut ve değişiklik zamanı listesiyle etiketlenir. Sahne eklenir, çıkarılır ya da güncellenirse etiket tutmaz ve karo yeniden üretilir. Önbellek ayardan bağımsızdır: mod, renk ve zemin ayarları görüntü üretilirken uygulanır.

Dosyalar `writeFileAtomic` ile yazılır: içerik önce benzersiz adlı bir geçici dosyaya yazılır, sonra yeniden adlandırılarak yerine konur. Okuyan taraf ya eski dosyayı ya da tamamlanmış yenisini görür. `CacheTrim` boyut sınırı aşıldığında en eski karoları birlikte siler ve bir saatten eski geçici dosyaları temizler.

## Görüntü üretimi

Kaba seviyeler sınıf rasterinden, yakın seviyeler doğrudan DSF üçgenlerinden boyanır. Ardından tek bir son adım her seviyede aynı formülü uygular:

- Kara rengi: Scenery'de sınıf rengi, VFR'de yükseklik bandı, IFR ve Dark'ta düz renk.
- Kabartma: her pikselde yükseklik verisinden bikübik türevle hesaplanır; dört yönlü ışık kullanılır.
- Su: kıyı çizgisi, kıyıdan açığa açık şerit ve derinliğe göre koyulaşma.
- Scenery'de sınıf sınırları konuma bağlı gürültüyle doğal kıvrım kazanır ve kara renkleri arasında yumuşak geçiş yapılır.

Yükseklik verisi bir piramit olarak tutulur; her piksel kendi boyuna uygun seviyeden okunur. Komşu karolarla ortak kenarlarda var olan karoların ortalaması alınır; böylece karo sınırında basamak oluşmaz.

## Yollar ve alanlar

`DsfReader` vektör poligonlarını ve ağları okur. `TerrainVectors` tanım adına göre sınıflandırır: orman, yerleşim, park; yollar otoyol, ana yol, tali yol, yerel yol, demiryolu ve enerji hattı. Overlay DSF'lerindeki havalimanı dışlama alanlarında yollar kesilir. Alanlar zemin dokusuna işlenir; yollar `RoadLayer` tarafından karo başına `StaticMesh` olarak çizilir.

## Öncelik ve paylaşım

İşçi, istekleri en son istenen önce, kaba seviye önce ve merkeze yakın önce olacak şekilde sıralar. Ekranın isteği yokken önceden yükleme ve bölge hazırlama listesindeki karoların disk önbelleğini hazırlar.

`TerrainHub` bir yöneticinin haritaları arasında zemin dokularını ve yol vektörlerini paylaştırır. Dokular GPU'da tek kopya tutulur ve onları son çiziminde kullanan haritalarla işaretlenir; bütçe aşılınca hiçbir haritanın kullanmadığı dokular en eskiden başlayarak silinir. Karede yüklenebilecek doku sayısı bütün haritalar için ortaktır. Farklı zemin ayarları farklı palet numaraları alır; aynı ayarlı haritalar dokuları paylaşır.

# Nav verisi, hava yolları, uçak

`NavLoader` X-Plane'in o an yüklü nav veritabanını SDK ile gezer. Dosya okumaz; böylece haritadaki navaid'ler simülatördekiyle aynıdır. `NavDatabase` kayıtları her tür için ayrı bir 1×1 derecelik ızgarada tutar.

`AirwayDatabase` `earth_awy.dat` dosyasını önce `Custom Data` klasöründen, yoksa varsayılan veriden okur. Satırlarda uç noktaların koordinatı yoktur; kimlik, bölge ve türle nav verisinden bulunur.

`AircraftData` uçağın konumunu, yönünü, hızını ve irtifasını dataref'lerden okur ya da host'un verdiği değeri kullanır.

SDK ile ilgili doğrulanmış noktalar: ILS yönü gerçek yöndür; TACAN frekansı kanal numarasıdır; `----` kimlikli yer tutucu ILS kayıtları atlanır.

# Çizim katmanı

```text
Katman / arayüz  ──►  Gfx::Mesh (işlemcide renkli üçgenler)  ──►  Canvas::draw
AirportSurfaceLayer, RoadLayer  ──►  Gfx::StaticMesh (GPU'da kalıcı)  ──►  Canvas::drawStatic
```

| Dosya | Görev |
|---|---|
| `gfx/Mesh` | Şekillerin işlemcideki hali. OpenGL'de çizgi kalınlığı güvenilir olmadığı için kalın çizgi, daire ve çerçeve üçgenlere çevrilir. `OutlinedLines` okunurluk için önce kontur, sonra renkli çizgi çizer. |
| `gfx/Canvas` | Tek çizim kapısı: VBO, VAO ve GLSL 330. Her çizimde X-Plane'in program, VAO, tampon ve yüz ayıklama durumunu saklayıp geri koyar. |
| `gfx/StaticMesh` | GPU'da kalıcı şekil; her karede yalnızca ölçek ve kaydırma gönderilir. |
| `gfx/RenderTarget` | Doku ve ona çizen framebuffer; Texture yolunun hedefi. |
| `gfx/ContextVertexArray` | VAO'lar GL bağlamları arasında paylaşılmaz; bağlam değişince yeniden oluşturulur. |
| `gfx/GlApi` | OpenGL 3.3 fonksiyonlarını ilk çizimde yükler. |
| `gfx/Texture` | Doku numarası `XPLMGenerateTextureNumbers` ile alınır, bağlama `XPLMBindTexture2d` ile yapılır. |
| `gfx/FontAtlas`, `ui/AtlasTextRenderer` | X-Plane'in `DejaVuSans.ttf` yazı tipini stb_truetype ile bir atlasa basar; yazılar toplu olarak tek çağrıda çizilir. |

## Texture yolu

`XMap::renderTexture()` şu sırayla çalışır:

1. Harita kapalı ya da gizliyse `false` döner.
2. Yeniden çizim gerekip gerekmediğine bakar: ilk çizim, doku boyutu, sahne imzası (mod, katmanlar, renkler, çizimler, arayüz), görünüm değişikliği ya da girdi. Hiçbiri yoksa ve hız sınırı dolmadıysa mevcut doku geçerlidir.
3. `RenderTarget` hazırlanır, `Canvas::beginOffscreen` framebuffer, viewport, scissor ve temizleme rengini saklar.
4. `XMapWindow::drawFrame` arka planı, katmanları, tooltip'i ve haritanın arayüzünü çizer.
5. `Canvas::endOffscreen` X-Plane'in durumunu geri koyar.

Girdiler host'tan `inject*` fonksiyonlarıyla gelir ve eski stillerdeki XPLM callback'lerinin kullandığı işleyicilere iletilir. Fare basıldığında önce arayüz parçaları denenir; dört pikselden az hareket tıklama, fazlası sürükleme sayılır; sağ tık yarım kalan rotayı ya da bölgeyi bitirir; tekerlek imlecin altındaki noktayı sabit tutarak zoom yapar.

## Eski pencere stilleri

`Standalone` ve `Embedded` stillerinde harita bir XPLM penceresine çizilir. Bu yolda `XMapImageCache` harita resmini bir framebuffer'da tutar ve değişmeyen karelerde yalnızca tek dokulu bir dörtgen basar. Texture yolunda bu önbellek kullanılmaz, çünkü harita zaten bir dokuya çizilir.

# Arayüz

Arayüz parçaları yalnızca çizer ve tıklamanın neye denk geldiğini söyler; ne yapılacağına `XMapWindow` karar verir. Ortak renk paleti ve çizim yardımcıları `ui/UiDraw`'dadır.

| Parça | Görev |
|---|---|
| `Toolbar` | Üst şerit: katmanlar, mod, uçak takibi, havalimanı arama, çizim araçları |
| `LayerPanel` | Gruplu, üç durumlu katman listesi |
| `ModePanel` | Harita modu seçimi |
| `ToolPalette` | Taşınabilir çizim paleti |
| `AirportSearchPanel` | Havalimanı arama kutusu ve sonuçları; açıkken klavye odağı ister |
| `StatusBar` | İmleç koordinatı, zoom, durum mesajı, ölçek çubuğu |

# Harita modları

`XMapTheme` beş modu tanımlar. Her tema arka plan ve su rengini, zemin biçimini, kontur rengini, katman renklerini ve mod başına varsayılan katman görünürlüğünü tutar.

| Mod | Zemin biçimi | Görünüm |
|---|---|---|
| Scenery | Arazi sınıfları ve kabartma | Koyu tema |
| VFR | Yükseklik bantları ve kabartma | Kuleli havalimanı mavi, kulesiz macenta; VOR gülü |
| IFR Low | Kara ve su | Açık tema; alçak hava yolları ve navaid'ler |
| IFR High | Kara ve su | Yalnızca büyük havalimanları ve yüksek hava yolları |
| Dark | Kara ve su, siyah kara | Varsayılan mod; havalimanları, pistler, navaid'ler, hava yolları |

Mod değiştiğinde önceki modun katman görünürlüğü saklanır, yeni modunki yüklenir. Zemin dokusunun anahtarı biçimi de içerir; her modun dokuları ayrı üretilir.

# Dosyalar ve paylaşım

| Dosya | İçerik |
|---|---|
| `prime_map.json` | Önbellek ve doku bütçeleri, önceden yükleme, otomatik kayıt |
| `maps/<ad>/map_settings.json` | Görünüm, mod, arayüz, katmanlar, zoom aralıkları, renkler, kaynaklar |
| `maps/<ad>/terrain_colors.json` | Zemin paleti ve ayarları |
| `maps/<ad>/map_drawings.txt` | Kullanıcı çizimleri |
| `cache/terrain/` | Zemin önbelleği |
| `cache/airports/airport_index.bin` | Havalimanı özet indeksi |

Harita ayarları bellekte `XMapWindow`'daki bir `Settings` deposunda tutulur; dosyaya okuma ve yazma `XMap`'tedir.

| Kurulum | Veri klasörü | Önbellek | Sonuç |
|---|---|---|---|
| Bir yönetici, birden çok harita | ortak | ortak | Veri ve dokular anında paylaşılır; ayarlar harita adına göre ayrıdır. |
| İki yönetici, aynı klasörler | ortak | ortak | Merkezi ayarlar ve aynı adlı haritalar çakışır; uyarı yazılır. Önerilmez. |
| İki yönetici, ayrı veri, ortak önbellek | ayrı | ortak | Ayarlar yalıtılmış, karolar tek havuzda. İki eklenti için önerilen kurulum. |
| İki yönetici, tamamen ayrı | ayrı | ayrı | Tam yalıtım; her yönetici karolarını kendisi üretir. |

Ayarlar `initialize()` sırasında bir kez okunur; iki haritayı eş zamanlı tutmak host'un işidir.

# Testler ve araçlar

| Araç | İçerik |
|---|---|
| `map_tests` | X-Plane olmadan çekirdeği dener: geometri, görünüm, etiket yerleşimi, ayarlar, çizimler, coğrafi hesaplar, `apt.dat` ayrıştırma, hava yolları, DSF okuma, zemin rasteri ve renk formülleri, yükseklik kodlaması, temalar, katman kuralları, zoom tablosu, atomik yazım. |
| `map_preview` | Kütüphanenin katman ve arayüz kodunu gerçek X-Plane verisiyle görünmez bir OpenGL penceresinde çizip resim dosyası yazar. `--bench` seçeneği katman başına çizim süresini ölçer. |
| `dsf_info` | Bir DSF dosyasının içeriğini döker. DSF biçimiyle ilgili değişiklikten önce gerçek dosya bununla incelenir. |
| `plugin-api-test` | X-Plane içinde API'nin bütün çağrılarını menüden dener; eklenti klasöründe `selftest.flag` varsa çoklu harita öz testini çalıştırır. |

# Performans yöntemleri

| Yöntem | Yer | Neden |
|---|---|---|
| 1×1 derecelik ızgara | `GeoGrid`, `NavDatabase`, `AirportGeometryStore` | Her karede yalnızca ekrandaki hücrelere bakılır. |
| Dilimli yükleme | `NavLoader` | SDK ana thread'den çağrıldığı için iş karelere bölünür. |
| İstekle hazırlama | `AirportDetailStore`, `TerrainStore` | Yavaş iş çizimi bekletmez. |
| İki seviyeli veri | `apt.dat` özeti ve ayrıntısı | Yalnızca özet bellekte durur. |
| Disk önbellekleri | `airport_index.bin`, `cache/terrain` | Pahalı tarama ve DSF açma bir kez yapılır. |
| LRU sınırları | Havalimanı ayrıntısı, zemin dokuları, üçgenler, rasterler | Bellek gezilen alanla sınırsız büyümez. |
| GPU'da kalıcı şekil | `StaticMesh` | Büyük şekiller bir kez yüklenir. |
| Hız sınırı | `renderTexture`, `XMapImageCache` | Değişiklik yokken harita sık çizilmez. |
| Toplu yazı | `AtlasTextRenderer` | Yüzlerce etiket tek çağrıda çizilir. |
| Doku yükleme sınırı | `TerrainHub` | Karede en çok üç doku yüklenir. |
| Zoom eşikleri | `kZoomTable` | Uzakta anlamsız ayrıntı işlenmez. |
| Hücreli çakışma testi | `LabelPlacer` | Etiket yalnızca değdiği hücrelerdeki kutularla karşılaştırılır. |
| Yükseklik piramidi | `TerrainElevation` | Uzak seviyeler önceden ortalanmış veriyi okur. |
