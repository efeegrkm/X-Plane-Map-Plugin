# Map Plugin 0.13 — X-Plane 11

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
| 0.8 | Zemin: kurulu DSF sahnesinden arazi + kabartma | X-Plane'de denendi |
| 0.9 | Modern OpenGL 3.3 (VBO/VAO/GLSL), C tarzı kodun temizliği | X-Plane'de denendi |
| 0.10 | Kendi apt.dat okuyucumuz: indeks + istek üzerine ayrıntı; havalimanı yüzeyleri, sınırlar, boya çizgileri | X-Plane'de denendi |
| 0.11 | Earcut third_party'de, indeks disk önbelleği, havalimanı nesneleri (kapı, helipad, beacon, tulum, PAPI, tabela), fare açıklaması | X-Plane'de denendi |
| 0.12 | Işık dizileri, taksi ağı katmanı, gruplu katman paneli | X-Plane'de denendi |
| 0.13 | Harita modları (Scenery / VFR / IFR Low / IFR High), VOR pusula gülü, sadece helipad'li heliport'lar | **denenmedi** |

Eski XPLMMap prototipi (0.3): `legacy/xplmmap_prototype/` (derlenmiyor).


Kod haritası: `MIMARI.md`. Başka bir yapay zekâ asistanı için devir notu: `AGENTS.md`.

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
| Next map mode | `ozdev/map/next_mode` (Scenery → VFR → IFR Low → IFR High → ...) |
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
| Layers | Gruplu katman listesi: grup kutusu gruptakilerin hepsini açar/kapatır (karışıksa çizgi), oka tıklayınca grup açılır |
| Map: ... | Harita modu listesi (aşağıda) |
| Follow aircraft / Center on aircraft | Uçağı takip et / bir kez uçağa ortala |
| Pan / Point / Route / Region | Çizim modu (Pan: sadece gezinme) |
| Finish / Undo / Clear | Yarım rota/bölgeyi bitir, son köşeyi/şekli geri al, hepsini sil |
| Pop out / Pop in | Pencereyi ayrı işletim sistemi penceresine al / geri koy |
| Hide | Kontrolleri gizle (imleç haritaya gelince sol üstte **Controls** düğmesi çıkar) |

**Alt şerit:** imlecin enlem/boylamı, zoom, ölçek çubuğu (NM) ve durum yazısı
(yükleme, çizim ipucu, teşhis modunda FPS).

## Harita modları

Araç şeridindeki **Map: ...** düğmesi veya `ozdev/map/next_mode`. Her modun
katman açık/kapalı durumu ayrı saklanır (`map_settings.ini`: `[layers.scenery]`,
`[layers.vfr]`, `[layers.ifr_low]`, `[layers.ifr_high]`; seçili mod `[view] mode`).
Renkler `src/MapTheme.cpp`'de; ilk sürüm FAA harita geleneklerinden,
X-Plane'in kendi haritasıyla karşılaştırılıp ayarlanacak.

| Mod | Zemin | Öne çıkanlar |
|---|---|---|
| Scenery | Arazi sınıfları + kabartma (koyu tema) | Önceki görünümün aynısı |
| VFR | Rakım renkleri (yeşil → sarı → kahverengi) + kabartma, su açık mavi | Kuleli havalimanı mavi, kulesiz macenta; VOR pusula gülü; Victor yolları kalın açık mavi; ILS ve fix kapalı |
| IFR Low | Sadece kara / su (iki düz renk, kıyılar görünür) | Alçak hava yolları, VOR/NDB/ILS/fix; büyükler yeşil, diğerleri kahverengi |
| IFR High | Aynı | Yüksek hava yolları, sadece büyük havalimanları; NDB, ILS, alçak yollar kapalı |

VOR gülü manyetik kuzeye döner: sapma, X-Plane'in VOR kaydındaki "slaved
variation" (SDK'nın verdiği yön alanı; Log'da `VOR sample:` satırları).

## Katmanlar

Alttan üste çizim sırası. Etiketler katman sırasından bağımsız bir öncelikle
yer bulur (havalimanı → pist numarası → VOR → NDB → ILS → fix → hava yolu);
hiçbir etiket bir sembolün üstüne binmez, sığmayan etiket zoom yapınca görünür.
Fareyi bir nesnenin üstüne getirince kısa açıklama çıkar (pist, yüzey, kapı,
navaid ...).

| Grup | Katman | Görünür | Not |
|---|---|---|---|
| Map | Terrain (scenery) | zoom 5+ | Kurulu DSF'lerden; biçim moda göre (yukarıda) |
| Map | Lat / lon grid | her zoom | Aralık zoom'a göre |
| Airports | Airport symbols | 4+ | Büyük 4+, küçük 8+, deniz üssü 9+, heliport 11+ (13+'da gerçek helipad), kapalı 11+ |
| Airports | Runways | 9+ | Gerçek ölçek; numaralar, blast pad, eşik, orta hat |
| Airports | Airport surfaces | 12+ | apt.dat: apron/taksi yolu yüzeyleri, sınır, boya çizgileri |
| Airports | Airport objects | 13+ | Helipad (yöne dönük H), beacon, rüzgâr tulumu, kapı/park (burun oku), PAPI, ışıklar 15+, tabelalar 16+ |
| Airports | Taxi routes (varsayılan kapalı) | 14+ | Taksi ağı (1201/1202), pist üstü kısım kırmızı, taksi yolu adları |
| Navaids | VOR / DME, NDB, ILS / LOC, Fixes | 5+ / 7+ / 8.5+ / 9+ | ILS tüyü pist eşiğinden |
| Airways | Airways (low) / (high) | 6.5+ | earth_awy.dat; adlar 8+ |
| Flight | Aircraft, FMS route, Flight track | her zoom | |
| — | User drawings | her zoom | Noktalar, rotalar (NM), bölgeler (NM²) |

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
- 0.13: önbellek dosyası sürüm 2 (VFR için rakım ızgarası eklendi). Eski
  önbellekteki karolar ilk açılışta bir kez yeniden üretilir (karo başına 1-3 s).

## apt.dat okuyucu

Ekip motoru kullanılmıyor (Qt bağımlılığı); okuyucu saf C++ (`src/data/Apt*`),
kaynak XP-APT-1100 spesifikasyonu. İki seviye:

1. **Özet indeksi** (açılışta, arka planda): bütün apt.dat'lar bir kez taranır
   (Custom Scenery `scenery_packs.ini` sırası, sonra Global; aynı havalimanında
   ilk gelen kazanır). Sonuç `cache/airports/airport_index.bin`'e yazılır;
   apt.dat'lar değişmedikçe sonraki açılışlar bu dosyadan okunur.
2. **Ayrıntı** (ekrana gelen havalimanı): işçi thread dosyanın o bayt aralığını
   okur, Bezier eğrilerini noktalara böler, yüzeyleri earcut
   (`third_party/earcut`) ile üçgenler; son 64 havalimanı bellekte tutulur.

## Eklenti klasöründeki dosyalar

| Dosya | Ne |
|---|---|
| `64/win.xpl` | Eklenti |
| `map_settings.ini` | Pencere yeri/boyutu, görünüm, takip, katmanların açık/kapalı durumu |
| `map_drawings.txt` | Kullanıcı çizimleri (metin; elle düzenlenebilir) |
| `terrain_colors.ini` | Zemin renkleri ve kabartma gücü |
| `cache/terrain/` | Zemin önbelleği (silinebilir) |
| `cache/airports/` | apt.dat indeks önbelleği (silinebilir) |

## Test listesi (0.13) — X-Plane'de denenecek

| # | Ne yap | Beklenen |
|---|---|---|
| 1 | X-Plane'i aç | Log'da `[MapPlugin] loaded (version 0.13)`; ilk açılışta zemin karoları bir kez yeniden üretilir (`terrain ...: built from`) |
| 2 | Map: Scenery → VFR | Zemin birkaç saniyede rakım renklerine döner; havalimanları mavi (kuleli) / macenta (kulesiz); VOR'ların çevresinde 10°'lik çentikli gül |
| 3 | IFR Low ve IFR High | Açık arka plan, su açık mavi, kıyılar görünüyor; IFR High'da sadece büyük havalimanları ve yüksek hava yolları |
| 4 | Her modda Layers'tan bir katmanı kapat, başka moda geç, geri gel | Her mod kendi katman ayarını hatırlıyor |
| 5 | Settings → Keyboard: `ozdev/map/next_mode`'a tuş ata | Tuş modları sırayla değiştiriyor |
| 6 | X-Plane'i kapatıp aç | Son seçilen mod ve katmanları geri geliyor |
| 7 | Sadece helipad'i olan bir heliport (Seattle şehir içi), zoom 13–17 | Yeşil daire içinde yönüne dönük H; zoom 13'ten sonra eski küçük H sembolü üst üste binmiyor |
| 8 | Log'da `VOR sample:` satırları | VOR başına sapma değeri (Seattle civarı ~+19); gülün kuzey oku sağa (doğuya) dönük |
| 9 | Diagnostics mode, her modda | FPS'i not et; modlar arasında belirgin fark olmamalı |

Sorun olursa: Diagnostics mode açıp aynı şeyi tekrarla ve Log.txt'yi gönder
(veya bana okut).

## Araçlar (X-Plane'siz; `-DMAP_PREVIEW=ON`)

- `map_preview`: eklentinin kendi katman kodunu gerçek X-Plane verisiyle bir
  resme çizer (zemin, pistler, navaid, hava yolları, çizimler, arayüz).
  `map_preview --lat 47.45 --lon -122.30 --zoom 11 --out ksea.ppm [--mode vfr] [--show taxi_routes]
  [--hide grid] [--panel Airports] [--describe x,y] [--demo-drawings]`
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
| `src/MapTheme.*` | Harita modları: renkler, zemin biçimi, mod başına varsayılan katmanlar |
| `src/ui/` | Toolbar, LayerPanel (gruplu), ModePanel, StatusBar, UiDraw |
| `src/gfx/` | OpenGL 3.3: Canvas (tek çizim kapısı), Mesh, StaticMesh (GPU'da kalıcı), Texture, GlApi |
| `src/data/NavDatabase.*`, `NavLoader.*` | X-Plane nav verisi (SDK) + ızgara |
| `src/data/Apt*.*`, `AirportGeometry*.*`, `AirportDetail*.*`, `AirportShapes.*` | apt.dat okuyucu: indeks, ayrıntı, çizim şekilleri |
| `src/data/LocalizerMatch.*` | ILS'i hizmet ettiği piste eşleme |
| `src/data/AirwayDatabase.*` | earth_awy.dat |
| `src/data/SceneryPacks.*`, `SceneryIndex.*` | Sahne paketi sırası, kurulu DSF dizini |
| `src/data/SevenZip.*`, `DsfReader.*` | 7z açma, DSF okuma |
| `src/data/TerrainClasses.*`, `TerrainRaster.*`, `TerrainStore.*` | Zemin sınıfları/renkler, raster + önbellek, arka plan işçisi |
| `src/Drawings.*`, `src/Geo.*`, `src/GeoGrid.*`, `src/LabelPlacer.*`, `src/Settings.*`, `src/BackgroundJob.*` | Çizim modeli, coğrafi hesap, ızgara, etiket seyreltme, INI, arka plan işi |
| `third_party/XPSDK`, `third_party/lzma`, `third_party/earcut` | X-Plane SDK; LZMA SDK 23.01 (kamu malı); mapbox earcut 2.2.4 (ISC, belgelenmiş tek hız yaması) |
| `tests/tests.cpp` | X-Plane'siz testler (355 kontrol) |
| `tools/preview`, `tools/dsfinfo` | Önizleme ve DSF inceleme araçları |

## Bilinen sınırlar

- Zemin 1°'lik karo başına 4096 px (~27 m enlem); çok yakın zoom'da (14+)
  pikseller büyür. Overlay DSF'lerdeki (havalimanı vb.) şekiller zemine çizilmez.
- Önbellek diskte büyüyebilir (~2 MB/karo); dolaşılan her karo için bir kez.
- Bina ve 3B nesneler (DSF overlay) yok; havalimanı yüzeyleri apt.dat'tan.
- Mod renkleri X-Plane'in haritasından örneklenmedi (FAA geleneği); MEF
  (bölge en yüksek irtifa) rakamları henüz yok.
- ILS yönü gerçek yön (T): X-Plane 11 SDK'sı manyetik yönü vermiyor.
- 180. boylam civarı kısmen ele alınıyor.
- Pop-out penceresinde imleç konumu cursor callback'ten okunuyor.
- Yazılar hâlâ XPLMDrawString (tek font, Türkçe harf yok).
