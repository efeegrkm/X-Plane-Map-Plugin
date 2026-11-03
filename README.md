# Map Plugin 0.8 — X-Plane 11

X-Plane içinde, **kendi penceresinde** çalışan, katmanları tek tek açılıp
kapatılabilen harita (AR-GE). Dış harita servisi (Kaşif, OSM) yok: zemin,
pistler, navaid'ler, hava yolları ve uçuş planı o bilgisayarda kurulu X-Plane
verisinden gelir. Bu yüzden haritada görünen her şey simülatördekiyle aynıdır;
yeni sahne kurulunca harita da kendiliğinden güncellenir.

| Sürüm | İçerik | Durum |
|---|---|---|
| 0.4 | Pencere, Web Mercator, kaydırma/zoom, ızgara, uçak, katman paneli, ayar kaydı | X-Plane'de denendi |
| 0.5 | Havalimanı, VOR/DME, NDB, ILS/LOC, fix, etiket seyreltme | X-Plane'de denendi |
| 0.6 | Pistler (apt.dat), ekip motoru bağlama arayüzü | X-Plane'de denendi |
| 0.7 | İngilizce arayüz, MapPlugin adı; ILS tüyü eşikten; heliport/deniz üssü; çizimler, uçuş izi, FMS; hava yolları | **denenmedi** |
| 0.8 | Zemin: kurulu DSF sahnesinden arazi + kabartma | **denenmedi** |

Eski XPLMMap prototipi (0.3): `legacy/xplmmap_prototype/` (derlenmiyor).

## Kurulum ve derleme

Qt Creator'da projeyi aç ve derle (kit: Desktop Qt 6.10.1 MSVC2022 64bit).
Derleme bitince CMake eklentiyi kendiliğinden kurar:

```
C:\X-Plane 11\Resources\plugins\MapPlugin\64\win.xpl
```

- **0.7'den itibaren klasör adı `MapPlugin`.** Eski `HaritaPlugin` klasöründeki
  `win.xpl` silindi, ayarlar `MapPlugin\map_settings.ini`'ye taşındı. Eski
  klasörde sadece eski README ve ikon kaldı; istersen klasörü sil.
- X-Plane açıkken `win.xpl` kilitlidir, kopyalama başarısız olur. Önce X-Plane'i
  kapat ya da Plugin Admin'den eklentiyi devre dışı bırak.
- CMake seçenekleri: `-DXPLANE_DIR=...` (farklı X-Plane klasörü),
  `-DMAP_DEPLOY=OFF` (kurma), `-DMAP_TESTS=ON` (X-Plane'siz testler,
  `map_tests.exe`), `-DMAP_PREVIEW=ON` (araçlar, aşağıda).

Log: `C:\X-Plane 11\Log.txt` içinde `[MapPlugin]` ile başlayan satırlar.

## Kullanım

**Menü: Plugins → Map Plugin**

| Öğe | Komut (Settings → Keyboard'da tuşa atanabilir) |
|---|---|
| Show / hide map | `ozdev/map/toggle` |
| Show / hide controls | `ozdev/map/controls` |
| Show / hide X-Plane map (compare) | X-Plane'in `sim/map/show_current` komutu: kendi haritası, yan yana karşılaştırmak için |
| Diagnostics mode | `ozdev/map/diagnostics` |
| Clear flight track | — |

**Fare**
- Sürükle: haritayı kaydırır (her modda; uçak takibini kapatır).
- Tekerlek: zoom; imlecin altındaki nokta yerinde kalır (takipteyken uçağın etrafında).
- Point / Route / Region modunda: **tıkla** = nokta / köşe ekle, **sağ tık** = rota/bölgeyi bitir.

**Üst şerit** (pencere darsa iki satıra çıkar)

| Düğme | Ne yapar |
|---|---|
| Layers | Katman listesi (onay kutuları) |
| Follow aircraft / Center on aircraft | Uçağı takip et / bir kez uçağa ortala |
| Pan / Point / Route / Region | Çizim modu (Pan: sadece gezinme) |
| Finish / Undo / Clear | Yarım rota/bölgeyi bitir, son köşeyi/şekli geri al, hepsini sil |
| Pop out / Pop in | Pencereyi ayrı işletim sistemi penceresine al / geri koy |
| Hide | Kontrolleri gizle (imleç haritaya gelince sol üstte **Controls** düğmesi çıkar) |

**Alt şerit:** imlecin enlem/boylamı, zoom, ölçek çubuğu (NM) ve durum yazısı
(yükleme, çizim ipucu, teşhis modunda FPS).

## Katmanlar

Alttan üste çizim sırası. Etiketler katman sırasından bağımsız bir öncelikle
yer bulur (havalimanı → pist numarası → VOR → NDB → ILS → fix → hava yolu);
hiçbir etiket bir sembolün üstüne binmez, sığmayan etiket zoom yapınca görünür.
FMS ve kullanıcı çizimi etiketleri her zaman çizilir.

| Katman | Görünür | Not |
|---|---|---|
| Terrain (scenery) | zoom 5+ | Kurulu DSF'lerden: su, şehir, orman, tarla, kaya, kar... + kabartma |
| Lat / lon grid | her zoom | Aralık zoom'a göre |
| ILS / LOC | 8.5+ | Tüy pist **eşiğinden** yaklaşma yönüne 10 NM; yakın zoom'da soluklaşır; yön "T" = gerçek |
| Runways | 9+ | Gerçek ölçek; 11.5: numaralar, 12: blast pad, 13: eşik + orta hat |
| Airways (low) / (high) | 6.5+ | earth_awy.dat; adlar 8+, tek yönlü segmentte ok 9+ |
| Airports | 4+ | Büyük (4 harf ICAO) 4+, küçük 8+, deniz üssü [S] 9+, heliport [H] 11+, kapalı [X] 11+ |
| VOR / DME | 5+ | Altıgen; VOR/DME altıgen+kare; DME kare; TACAN frekansı `CH85` |
| NDB | 7+ | Noktalı çember |
| Fixes (varsayılan kapalı) | 9+ | Üçgen |
| FMS route | her zoom | X-Plane FMS planı, saniyede bir okunur; aktif bacak parlak |
| Flight track | her zoom | Saniyede bir örnek; 20 NM'den büyük sıçramada yeni parça |
| Aircraft | her zoom | Sarı uçak, burun yönünde |
| User drawings | her zoom | Noktalar (P1), rotalar (R1 + NM), bölgeler (A1 + NM²) |

## Zemin (Terrain) nasıl çalışıyor

- Eklenti, X-Plane'in yaptığı gibi her 1°×1° karo için sahne paketlerini öncelik
  sırasıyla tarar: önce `Custom Scenery` (scenery_packs.ini'deki etkin sıra),
  sonra `Global Scenery`. **Overlay olmayan ilk DSF** karonun zeminidir
  (Ortho4XP gibi özel bir mesh kurarsan onun zemini görünür).
- DSF'ler 7z ile sıkıştırılmıştır; eklenti kendisi açar (LZMA SDK, kamu malı),
  arazi üçgenlerini 4096×4096'lık bir sınıf haritasına boyar ve DEM'den kabartma
  hesaplar. Bu iş arka planda yapılır (karo başına ~0.5–1 s), sonuç
  `MapPlugin\cache\terrain\` altına yazılır (~2 MB/karo).
- **Senkron kalma:** önbellekteki her karo, üretildiği DSF'lerin
  yol + boyut + değişiklik zamanı listesiyle etiketlidir. Sahne eklenir,
  çıkarılır, sırası değişir veya güncellenirse etiket tutmaz ve karo yeniden
  üretilir. Önbelleği silmek hiçbir şeyi bozmaz (sadece yeniden üretilir).
- DSF'i olmayan karo: bölgesi (10°×10° klasör) bir Global Scenery paketinde
  kuruluysa **deniz** (Laminar tamamen deniz olan karoları dağıtmaz), değilse
  **veri yok** (koyu zemin).
- Renkler: `MapPlugin\terrain_colors.ini` (ilk açılışta varsayılanlarla
  oluşur): `[colors]` altında sınıf başına `R G B` (0–255), `[shading] strength`
  0 (düz) … 1 (güçlü kabartma). Değişiklik X-Plane yeniden açılınca uygulanır;
  önbelleği silmek gerekmez.

## apt.dat motorunu bağlama (ekip motoru)

Pist katmanı apt.dat'ı bilmez; sadece `IAirportGeometrySource` arayüzünü
bilir. Şu an onu **geçici** okuyucu `src/data/AptDatReader.*` uyguluyor.

1. `IAirportGeometrySource`'tan türeyen bir adaptör yaz (örnek ad
   `src/data/TeamAptDatSource.*`). `LoadAll()` içinde motoru çalıştır, sonucu
   `AirportGeometry` / `Runway` / `RunwayEnd` yapılarına doldur (derece, metre).
2. `src/Plugin.cpp`'de **MOTOR BAGLAMA NOKTASI** yazan fonksiyondaki tek satırı
   kendi sınıfınla değiştir.
3. Yeni `.cpp`'yi (ve motorun dosyalarını) `CMakeLists.txt`'de `PLUGIN_SOURCES`'a ekle.

Kurallar ve veri yapıları `src/data/AirportGeometry.h`'nin başında. En önemlisi:
`LoadAll` arka plan thread'inde çalışır; içinde XPLM fonksiyonu (Log dahil)
çağrılmaz, log'a yazılacaklar `pOutReport`'a eklenir. `tests/tests.cpp`'deki
`FakeAirportSource` en küçük örnek uygulamadır.

## Eklenti klasöründeki dosyalar

| Dosya | Ne |
|---|---|
| `64/win.xpl` | Eklenti |
| `map_settings.ini` | Pencere yeri/boyutu, görünüm, takip, katmanların açık/kapalı durumu |
| `map_drawings.txt` | Kullanıcı çizimleri (metin; elle düzenlenebilir) |
| `terrain_colors.ini` | Zemin renkleri ve kabartma gücü |
| `cache/terrain/` | Zemin önbelleği (silinebilir) |

## Test listesi (0.7 + 0.8) — X-Plane'de denenecek

| # | Ne yap | Beklenen |
|---|---|---|
| 1 | X-Plane'i aç | Plugins menüsünde **Map Plugin** (tek bir tane). Log'da `[MapPlugin] loaded (version 0.8)`, `terrain index: ... DSF files`, `nav data ready`, `runway data ready`, `airways ready` |
| 2 | Map Plugin → Show / hide map | Pencere eski yerinde, başlık "Map"; tüm yazılar İngilizce |
| 3 | Seattle, zoom 7–9 | Zemin birkaç saniyede belirir (alt şeritte "Building terrain ..."): Puget Sound mavi, şehir gri, orman yeşil, dağlar kabartmalı, Rainier karlı |
| 4 | Show / hide X-Plane map (compare) ile kendi haritasını yan yana aç | Kıyı çizgisi, göller, pistler aynı yerde mi |
| 5 | KSEA, zoom 12–14 | Koyu pistler beyaz kenarlı; ILS tüyleri pist eşiğinden dışarı uzanıyor, pistleri örtmüyor |
| 6 | Zoom 11, Seattle şehir içi | Heliportlar küçük "H" sembolü; adları ancak 14'te |
| 7 | Point → haritaya birkaç tık | P1, P2... Sürükleyince harita kayar, nokta eklenmez |
| 8 | Route → 3-4 tık, sağ tık | Mavi rota, "R1 xx NM"; çizerken turuncu ve imlece kesikli çizgi |
| 9 | Region → 4-5 tık, Finish | Yeşil dolu bölge, "A1 xx NM2" |
| 10 | Undo, Clear; X-Plane'i kapatıp aç | Geri alma çalışıyor; çizimler yeniden açılınca geri geliyor |
| 11 | Biraz uç (veya konum değiştir) | Sarı uçuş izi; Map Plugin → Clear flight track ile silinir |
| 12 | FMS'e 3-4 nokta gir | Mor rota, aktif bacak parlak, nokta adları |
| 13 | Zoom 7–9, Layers → Airways (low/high) | Yeşil (V/T) ve mavi (J/Q) hava yolları, adlar |
| 14 | Pencereyi daralt | Araç şeridi iki satıra çıkar, harita alanı ona göre küçülür |
| 15 | Diagnostics mode, fix ve hava yolları açık, yoğun bölge | Alt şeritte FPS ve "draw x ms": değerleri not et |
| 16 | Avrupa/Türkiye'ye kaydır (demo kurulumda sahne yok) | Zemin "veri yok" (koyu), navaid/havalimanları yine görünür |

Sorun olursa: Diagnostics mode açıp aynı şeyi tekrarla ve Log.txt'yi gönder
(veya bana okut).

## Araçlar (X-Plane'siz; `-DMAP_PREVIEW=ON`)

- `map_preview`: eklentinin kendi katman kodunu gerçek X-Plane verisiyle bir
  resme çizer (zemin, pistler, navaid, hava yolları, çizimler, arayüz).
  `map_preview --lat 47.45 --lon -122.30 --zoom 11 --out ksea.ppm [--demo-drawings]`
  (`magick ksea.ppm ksea.png` ile PNG'ye çevrilir).
- `dsf_info`: bir DSF'in özeti (özellikler, arazi türleri, üçgen sayıları);
  `--raw out.dsf` ile açılmış halini yazar.

## Dosyalar

| Dosya | Görev |
|---|---|
| `src/Plugin.cpp` | Giriş noktaları, menü, komutlar, flight loop, katman sırası, **motor bağlama noktası** |
| `src/MapWindow.*` | Pencere, fare (sürükleme/tıklama/sağ tık/tekerlek), çizim sırası, ayar ve çizim dosyası |
| `src/MapView.*` | Web Mercator projeksiyonu |
| `src/Layer.h`, `src/LayerManager.*` | Katman arayüzü, liste, görünürlük kaydı, etiket önceliği |
| `src/layers/` | Terrain, Grid, Runway, Airway, Airport, Navaid (VOR/NDB/ILS/Fix), Fms, Track, Aircraft, Drawing |
| `src/ui/` | Toolbar, LayerPanel, StatusBar, UiDraw |
| `src/gfx/Batch.*`, `src/gfx/Texture.*` | Toplu çizim ve doku: OpenGL çağrılarının toplandığı yer |
| `src/data/NavDatabase.*`, `NavLoader.*` | X-Plane nav verisi (SDK) + ızgara |
| `src/data/AirportGeometry*.*`, `AptDatReader.*` | Pist verisi, motor arayüzü, **geçici** apt.dat okuyucu |
| `src/data/LocalizerMatch.*` | ILS'i hizmet ettiği piste eşleme |
| `src/data/AirwayDatabase.*` | earth_awy.dat |
| `src/data/SceneryPacks.*`, `SceneryIndex.*` | Sahne paketi sırası, kurulu DSF dizini |
| `src/data/SevenZip.*`, `DsfReader.*` | 7z açma, DSF okuma |
| `src/data/TerrainClasses.*`, `TerrainRaster.*`, `TerrainStore.*` | Zemin sınıfları/renkler, raster + önbellek, arka plan işçisi |
| `src/Drawings.*`, `src/Geo.*`, `src/GeoGrid.*`, `src/LabelPlacer.*`, `src/Settings.*`, `src/BackgroundJob.*` | Çizim modeli, coğrafi hesap, ızgara, etiket seyreltme, INI, arka plan işi |
| `third_party/XPSDK`, `third_party/lzma` | X-Plane SDK; LZMA SDK 23.01 (kamu malı) |
| `tests/tests.cpp` | X-Plane'siz testler (275 kontrol) |
| `tools/preview`, `tools/dsfinfo` | Önizleme ve DSF inceleme araçları |

## Bilinen sınırlar

- Zemin 1°'lik karo başına 4096 px (~27 m enlem); çok yakın zoom'da (14+)
  pikseller büyür. Overlay DSF'lerdeki (havalimanı vb.) şekiller zemine çizilmez.
- Önbellek diskte büyüyebilir (~2 MB/karo); dolaşılan her karo için bir kez.
- Taksi yolu / apron / bina yok (geçici okuyucu sadece pistleri okuyor).
- ILS yönü gerçek yön (T): X-Plane 11 SDK'sı manyetik yönü vermiyor.
- 180. boylam civarı kısmen ele alınıyor.
- Pop-out penceresinde imleç konumu cursor callback'ten okunuyor.
- Çizim OpenGL'i: X-Plane 11 eklentilere uyumluluk (eski) profili verdiği için
  glBegin/glEnd; hepsi `gfx/` ve birkaç sembol yardımcısında toplandı, ileride
  vertex buffer'a geçiş tek yerden yapılabilir.
