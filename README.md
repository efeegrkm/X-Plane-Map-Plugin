# Prime Map Plugin (prime-map-plugin) 0.18 — X-Plane 11

X-Plane içinde, **kendi penceresinde** çalışan, katmanları tek tek açılıp
kapatılabilen harita. Dış harita servisi (Kaşif, OSM) yok: zemin, pistler,
havalimanı yüzeyleri, navaid'ler, hava yolları ve uçuş planı o bilgisayarda
kurulu X-Plane verisinden gelir. Bu yüzden haritada görünen her şey
simülatördekiyle aynıdır; yeni sahne kurulunca harita da kendiliğinden
güncellenir. Hedef kullanım: simülatörlerin eğitmen istasyonu (IOS) ekranları.

| Belge | Kimin için |
|---|---|
| `README.md` (bu dosya) | Kullanım, kurulum, katmanlar, ayar dosyaları, test listesi |
| `MIMARI.pdf` (kaynağı `docs/MIMARI.html`) | Kodu anlamak isteyen geliştirici: mimari, veri akışı şemaları, dosya görevleri |
| `AGENTS.md` | Projede çalışacak yapay zekâ asistanı: kurallar, kod standardı, derleme, doğrulanmış gerçekler, durum |

## Kurulum ve derleme

Qt Creator'da projeyi aç ve derle (kit: Desktop Qt 6.10.1 MSVC2022 64bit; Qt
kütüphanesi kullanılmıyor, sadece MSVC + CMake + Ninja). Derleme bitince CMake
eklentiyi kendiliğinden kurar:

```
C:\X-Plane 11\Resources\plugins\prime-map-plugin\64\win.xpl
```

- X-Plane açıkken `win.xpl` kilitlidir, kopyalama başarısız olur. Önce X-Plane'i
  kapat ya da Plugin Admin'den eklentiyi devre dışı bırak.
- CMake seçenekleri: `-DXPLANE_DIR=...` (farklı X-Plane klasörü),
  `-DMAP_DEPLOY=OFF` (kurma), `-DMAP_TESTS=ON` (X-Plane'siz testler,
  `map_tests.exe`), `-DMAP_PREVIEW=ON` (araçlar, aşağıda).
- Komut satırından derleme: `AGENTS.md` bölüm 5.

Log: `C:\X-Plane 11\Log.txt` içinde `[prime-map-plugin]` ile başlayan satırlar.

## Kullanım

**Menü: Plugins → Prime Map Plugin**

| Öğe | Komut (Settings → Keyboard'da tuşa atanabilir) |
|---|---|
| Show / hide map | `ozdev/map/toggle` |
| Show / hide controls | `ozdev/map/controls` |
| Next map mode | `ozdev/map/next_mode` (Scenery → VFR → IFR Low → IFR High → Dark → ...) |
| Show / hide X-Plane map (compare) | X-Plane'in `sim/map/show_current` komutu: yan yana karşılaştırmak için |
| Diagnostics mode | `ozdev/map/diagnostics` |
| Clear flight track | — |

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
| Layers | Katman paneli: grup › katman › alt katman. Kutu altındakilerin hepsini açar/kapatır (karışıksa çizgi), ok listeyi açar (varsayılan kapalı) |
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

**Map: ...** düğmesi veya `ozdev/map/next_mode`. Her modun katman ve alt katman
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
| `map_settings.ini` | Pencere yeri/boyutu, görünüm, seçili mod, çizim paleti, mod başına katman ve alt katmanlar, `[cache] terrain_mb` (zemin önbelleği sınırı, varsayılan 1024; 0 = sınırsız) |
| `map_drawings.txt` | Kullanıcı çizimleri (metin; elle düzenlenebilir) |
| `terrain_colors.ini` | Zemin renkleri ve ayarları (yukarıda) |
| `cache/terrain/` | Zemin önbelleği (silinebilir): `*.terrain` sınıflar ve alanlar, `*.elevation` rakım, `*.vectors` DSF vektörleri, `xplane_palette.txt`. Karo başına ~3–4 MB; `terrain_mb` aşılınca en uzun süredir kullanılmayan karolar (tüm dosyalarıyla) silinir, toplam sınırın %90'ına iner |
| `cache/airports/` | apt.dat indeks önbelleği (silinebilir) |

## Test listesi (0.18) — X-Plane'de denenecek

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
