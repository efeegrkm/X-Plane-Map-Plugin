# X-Plane Harita Plugini — Proje Notu

## Amaç
X-Plane 11 içinde, Kaşif / OSM / dış harita servisi kullanmadan, sadece
X-Plane'in kendi verisi + OpenGL ile, **kendi penceresinde** çalışan ve
katmanları tek tek açılıp kapatılabilen bir harita (AR-GE). Ayrıntılı görev
tanımı, kararlar ve aşamalar: **`GOREV.md`** (önce onu oku). Kullanım, test
listesi ve dosya görevleri: `README.md`.

## Çalışma şekli
- Kullanıcıyla Türkçe konuş. Kullanıcı X-Plane plugin geliştirmeye yeni,
  kodu ANLAYARAK ilerlemek istiyor: her aşama sonunda ne yapıldığını ve
  nedenini sade anlat.
- Her aşama sonunda: derle, testleri çalıştır, uyarıları temizle, bu dosyayı
  güncelle. "X-Plane'de dene" noktalarında test listesi ver, sonucu bekle.
- X-Plane'de denenmemiş hiçbir şeyi "çalışıyor" diye raporlama.
- Biçim tahmin etme: DSF / earth_awy.dat / apt.dat için önce gerçek dosyaya
  bak (`tools/dsfinfo`, dosyanın kendi satırları).
- Git: `origin` = github.com/efeegrkm/X-Plane-Map-Plugin, dal `main`. İş
  bitince commit + push (kullanıcı istedi). Commit mesajları İngilizce.

## Kod standardı (kullanıcının isteği, mutlaka uy)
- Önekler: `m` üye değişken (`mZoom`), `p` parametre (`pZoom`), `t` yerel /
  geçici değişken (`tZoom`, döngü sayaçları dahil: `tIndex`), `k` sabit
  (`kMaxZoom`), `g` global (`gPlugin`, `gCrcTableOnce`). Yapı (struct)
  alanları da `m` önekli (`LatLon::mLat`).
- Süslü parantez her zaman **alt satırda** açılır (sınıf, fonksiyon,
  namespace, if/else, döngü, switch, lambda). Tek satırlık `if` bile parantezli.
- Tür ve fonksiyon adları PascalCase (`MapView::ZoomAt`). Satır en çok 120 karakter.
- **Her şey İngilizce**: arayüz metinleri, log, menü, komut, ayar anahtarları,
  dosya adları, katman kimlikleri. **İstisna:** kod yorumları Türkçe, ASCII
  (ş/ğ/ı yerine s/g/i); README.md ve CLAUDE.md Türkçe.
- XPLM callback'lerine refcon olarak `this` verilir; global durum yok.

## Sabitler
- X-Plane 11.55r2 demo, Windows x64, `C:\X-Plane 11`. SDK projede:
  `third_party/XPSDK` (sadece XPLM200..XPLM301 tanımlı; XP12 API'si derlenmez).
- C++20 (std::format, std::to_array), CMake + MSVC 2022 (Qt Creator kiti "Desktop Qt 6.10.1 MSVC2022
  64bit", Ninja; Qt kütüphanesi kullanılmıyor). /W4 uyarısız, statik CRT
  (`CMAKE_MSVC_RUNTIME_LIBRARY` MultiThreaded), diller C + CXX.
- Komut satırından derleme: vcvars64 + `C:\Qt\Tools\CMake_64\bin\cmake.exe`
  + `C:\Qt\Tools\Ninja\ninja.exe`. Kullanıcının `build/` klasörüne dokunma;
  kendi derlemeni scratchpad'de yap. Bash heredoc'u `\n`'i bozuyor: kaynak
  dosyaya dizgi yazarken Write ile Python betiği yaz, `python -I` ile çalıştır.
- Kurulum: CMake (`MAP_DEPLOY=ON`) `win.xpl`'yi
  `C:\X-Plane 11\Resources\plugins\MapPlugin\64\` altına kopyalar.
  X-Plane açıkken dosya kilitli olur (önce `tasklist` ile kontrol et).
  Eski `HaritaPlugin` klasöründe sadece eski README + ikon kaldı (etkisiz).
- Eklenti klasörü dosyaları: `map_settings.ini` (window/view/layers),
  `map_drawings.txt`, `terrain_colors.ini`, `cache/terrain/*.terrain`.
- Kurulu sahne SADECE `Global Scenery/X-Plane 11 Demo Areas` (35 DSF: Hawaii,
  Seattle +46..+48 / -121..-124, Juneau) + Global Airports overlay'leri.
  Türkiye YOK. DSF'ler 7z sıkıştırılmış.
- Dokunma: `_qt_template_yedek/`, `_to_delete/`, `build/`, `.qtcreator/`.
  X-Plane klasöründe sadece `Resources\plugins\MapPlugin\` altına yaz.

## apt.dat kararı (kullanıcı, 2026-11-04; öncekinin yerine)
- Ekibin Qt tabanlı motoru kullanılmayacak (Qt bağımlılığı doğuruyor,
  veritabanı katmanı yavaş). apt.dat okuyucuyu biz saf C++ ile yazıyoruz.
  Kaynak: XP-APT-1100 spesifikasyonu (kullanıcı verdi; ekip motorunu da buna
  göre yazmıştı). Ölçülen Global Airports apt.dat: 325 MB, 29.856 havalimanı,
  7,25 milyon düğüm (111-116), 156 bin yüzey (110), 881 bin çizgi (120).
- 0.10'da yazıldı (aşağıda "apt.dat okuyucu"). Spesifikasyon PDF'i:
  `C:\Users\Asus\Downloads\XP-APT-1100-Spec_rev_16_11_2021.pdf` (21 sayfa).

## Zemin kararı (kullanıcı)
- Zemin o bilgisayarda kurulu HER sahneyle çalışmalı (bölgeye özel iş yok),
  özel sahne global'i ezmeli, güncellemeler kendiliğinden yansımalı.
- Bu yüzden GOREV.md'deki "Python ile offline karo aracı" yerine DSF okuyucu
  eklentinin içinde (aşama 6): `SceneryIndex` X-Plane'in sırasını taklit eder
  (Custom Scenery `scenery_packs.ini` sırası, sonra Global Scenery; overlay
  olmayan ilk DSF zemin). Önbellek anahtarı = karonun TÜM aday DSF'lerinin
  yol|boyut|mtime listesi: değişirse karo yeniden üretilir.
- DSF'i olmayan karo: 10°'lik klasörü bir Global paketinde varsa deniz, yoksa
  veri yok. Sadece overlay DSF'i olan karo (Global Airports) için `.nobase`
  işareti yazılır, deniz sayılır.

## Mimari (sürüm 0.8)
- `src/Plugin.cpp`: giriş noktaları, menü "Map Plugin", komutlar
  (`ozdev/map/toggle|controls|diagnostics`), flight loop (nav yükleme, pist /
  hava yolu / zemin arka plan işleri, log aktarımı, durum yazısı). Katman
  sırası `CreateLayers()`'ta: Terrain, Grid, ILS, Runways, Airways low/high,
  Airports, VOR, NDB, Fix, FMS, Track, Aircraft, Drawings.
- `MapWindow`: `XPLMCreateWindowEx` (pop-out destekli). Fare: 4 px'ten az
  hareket = tıklama, fazlası = sürükleme; sağ tık rota/bölgeyi bitirir;
  tekerlek imleç etrafında zoom. Çizimler `map_drawings.txt`.
  Scissor için boxel->piksel her çizimde GL matrisinden okunur (`GlScreen`).
- `MapView`: Web Mercator, merkez Mercator biriminde (0..1), sürekli zoom.
- `Layer` + `LayerManager`: görünürlük `[layers]`'a kaydedilir; etiketler
  `Layer::LabelPriority()` sırasıyla yerleşir (Airport 10, Runway 15, VOR 20,
  NDB 30, ILS 40, Fix 60, Airways 70). Semboller `LabelPlacer::Reserve` ile
  kutularını ayırır.
- `ui/`: `Toolbar` (dar pencerede iki satır, `kRowHeight`), `LayerPanel`,
  `StatusBar`, `UiDraw` (`Ui::k...` paleti, kullanıcı beğendi; yeni arayüz
  parçaları aynı paletle).
- **Çizim (0.9, modern OpenGL 3.3):** `gl` çağrısı SADECE `src/gfx/`
  içinde. `gfx/Mesh` (CPU, testli): her şey renkli üçgen (kalın çizgi,
  daire, çerçeve; kalınlıklar boxel). `gfx/OutlinedLines`: önce koyu
  kontur, sonra renkli çizgi. `gfx/Canvas`: tek çizim kapısı (VBO + VAO +
  GLSL 330; her Draw'da X-Plane'in program/VAO/VBO/cull face durumunu
  kaydedip geri koyar; matris = GL_PROJECTION × GL_MODELVIEW, BeginFrame'de
  okunur). `gfx/GlApi`: 3.3 fonksiyonlarını wglGetProcAddress ile yükler
  (ilk draw callback'inde). `gfx/Texture`: RAII doku (XPLMBindTexture2d).
  Yazılar hâlâ XPLMDrawString (`Ui::Text`). Katmanlar `pContext.mCanvas`'ı,
  arayüz parçaları `Gfx::Canvas&` parametresini kullanır.
- Log: `Log("x = {:.1f}", tX)` (std::format); arka plan thread'inde yok.
- **apt.dat okuyucu (0.10):** iki seviye.
  1. Özet: `data/AptDatIndexer` (+ `AptDatScanner`) açılışta tüm apt.dat'ları
     4 MB parçalarla bir kez tarar (0,8 s, 37 bin havalimanı, 7,26 M düğüm):
     başlık, pistler (100/101), kapsayan kutu, kule (54/1054), 1302
     icao_code/datum, dosya + bayt aralığı (`AptFileLocation`). Sıra:
     scenery_packs.ini, sonra default apt.dat; aynı kimlikte ilk gelen kazanır.
     Disk önbelleği (0.11, kullanıcı istedi): `cache/airports/airport_index.bin`
     (9 MB), anahtar = her apt.dat yolu|boyut|mtime; tarama 0,82 s -> 0,03 s.
  2. Ayrıntı: `data/AirportDetailStore` (işçi thread, LRU 64; katmanlar
     `Use()` ile ister, plugin flight loop'u her karede `Update()` çağırır): ekrandaki
     havalimanının bayt aralığına seek edip `AptDatParser::ParseAirport` ile
     `AirportDetail`'e (tüm satır kodları), `Shapes::Build` ile çizim
     şekillerine çevirir.
  - `AptLine`: yerel ayardan bağımsız alan okuyucu. Üçgenleme: dış kütüphane
    `third_party/earcut/mapbox/earcut.hpp` (v2.2.4, ISC; kullanıcı kuralı:
    dış kod third_party'de, kendi kodumuza karıştırılmaz). TEK belgelenmiş
    yama (isValidDiagonal sırası, sonuç aynı; README.txt): WAHI 2,6 -> 0,5 s,
    tüm dünya 38,5 -> 21,7 s. Üçgen disk önbelleği bilinçli olarak YOK
    (en kötü 0,6 s; kullanıcı kararı bana bıraktı).
  - `AirportShapes` noktaları Mercator farkı (float, referansa göre) →
    `layers/AirportSurfaceLayer` bunları bir kez `Gfx::StaticMesh`'e yükler,
    her karede `Canvas::DrawStatic` (uTransform: ölçek + kaydırma).
  - `layers/AirportObjectsLayer` (0.11): kalkış/park (1300/15, burun yönüne
    ok; kapı/bağlama/hangar/diğer ayrı), helipad (yöne dönük H), beacon,
    rüzgar tulumu, PAPI/VASI/wig-wag, tabela. Ortak `layers/AirportVisibility`.
  - 0.12: ışık dizileri (101-108, 15 m arayla, `AirportShapes::mLights`),
    `layers/TaxiRouteLayer` (1201/1202, varsayılan kapalı, taksi yolu adları),
    gruplu katman paneli (`ui/LayerPanel`, gruplar `kGroups` tablosunda:
    Map, Airports, Navaids, Airways, Flight; üç durumlu kutu, ok ile aç/kapa).
  - Fare açıklaması (0.11): `Layer::Describe` + `LayerManager::DescribeAt`,
    `MapWindow::DrawTooltip`. Pist, yüzey, nesne, havalimanı, navaid
    katmanları açıklama veriyor. Önizlemede `--describe x,y` ile sınanır.
- Ölçümler (Global Airports, bu PC): tüm havalimanları ayrıştır + üçgenle
  25 s toplam; en yavaş PANC 581 ms, YSSY 200 ms, KORD 53 ms; 156.345
  yüzeyden 1'i üçgenlenemiyor; reddedilen satır 0.
- Teşhis araçları (scratchpad'de, depoda değil): AptDebug (tek havalimanı,
  yüzey yüzey üçgenleme + PGM resmi), Bench (en büyükler / "all").
- Veri: `NavDatabase` + `NavLoader` (XPLMNavigation, flight loop'ta 4 ms
  adımlar), `AirportGeometryStore`, `LocalizerMatch` (ILS -> pist eşiği),
  `AirwayDatabase` (earth_awy.dat 1100; Custom Data önce; 104.581 segment,
  hepsi çözülüyor), `BackgroundJob`.
- Zemin: `SevenZip` (LZMA SDK 23.01, `third_party/lzma`), `DsfReader`,
  `SceneryPacks`, `SceneryIndex`, `TerrainClasses` (ada göre sınıf + palet),
  `TerrainRaster` (4096² sınıf + 1024² gölge, RLE önbellek), `TerrainStore`
  (işçi thread, LOD 64/256/1024/4096-parça), `layers/TerrainLayer` (min zoom
  5, LRU 256 MB, karede en çok 3 doku yükleme).
- Arka plan thread'lerinde XPLM/Log YOK: rapor metni döner, ana thread loglar.
- X-Plane'siz çekirdek testlere girer (`tests/tests.cpp`, 291 kontrol);
  `tools/preview` aynı katman kodunu XPLM taklitleriyle resme çizer.

## Doğrulanmış biçim / SDK gerçekleri
- SDK: ILS yönü GERÇEK yön (manyetik yok); TACAN frekansı kanal numarası
  (NUW = 85); "----" kimlikli yer tutucu ILS kayıtları var (filtreleniyor).
- apt.dat GERÇEK VERİ ≠ spesifikasyon (kodda "GERCEK VERI" notlarıyla):
  düğüm sonunda 0-3 stil değeri ("51 59 105" = 2 boya + ışık; 1-99 boya,
  101-108 ışık); 21 satırı 6 alan; 1202'de "taxiway_A".."F"; 1300 tipi
  "tie_down"; 14 yüksekliği ondalık; başlıktaki kule alanı hep 0 (kule =
  54/1054 satırı); dosya ortasında "1130 Generated by..." satırı olabilir.
  Bezier kuralı (spesifikasyonda yok, ölçüldü: 2000 halkada 78'e karşı
  1719 kendini kesme): kontrol noktası düğümden ÇIKAN eğriye ait, gelen
  eğri için aynası (2P − C).
- DSF (XP11 demo dosyalarında görüldü): POOL/SCAL değeri =
  ham/65535*ölçek+kaydırma; nested polygon range komutu sayaç+1 indeks okur;
  DEM "elevation" 1201², int16, satır 0 = GÜNEY, sütun 0 = batı.
- Pistler zemin üstünde koyu dolgu + açık kenar (eski açık gri, zeminde
  kayboluyordu); 2 px'ten dar pist açık renkli çizgi.

## Prototipten dersler (GOREV.md Bölüm 4)
- GL_CULL_FACE kapat (`CullFaceOff`), `XPLMSetGraphicsState` her geometri
  çiziminden önce (`Ui::UseFlatState`), çünkü `XPLMDrawString` doku açıyor.
- Tıklamayı alttaki pencereye geçirme (fare takibi bozuluyor).

## Durum
- 0.4, 0.5, 0.6: X-Plane'de denendi (kullanıcı beğendi; 0.5 kusurları 0.6'da
  düzeltildi). FPS ölçümü henüz yapılmadı.
- 0.7 (İngilizce arayüz, MapPlugin adı, ILS eşikten, heliport/deniz üssü,
  aşama 4 çizimler/iz/FMS, aşama 5b hava yolları) ve 0.8 (aşama 6 zemin):
  derlendi, 275 test geçiyor, X-Plane'e kuruldu. 2026-11-04 kullanıcı
  X-Plane'de denedi (ekran görüntüsü): zemin (Seattle), çizimler (nokta /
  rota / bölge), katman paneli, uçak, hava yolları, ILS tüyleri görünüyor.
  Test listesinin geri kalanı (kalıcılık, FMS, FPS...) raporlanmadı.
- Kullanıcının geri bildirimi: havalimanı yüzeyleri yok (ekip motoruyla
  yapılacak); harita X-Plane'inkinden farklı, X-Plane gibi harita modları
  istiyor; OpenGL hâlâ eski (glBegin/glEnd) ve kod fazla C tarzı.

- 0.9 (2026-11-04): OpenGL 3.3'e geçiş + C tarzı temizlik. Kullanıcı
  X-Plane'de denedi: sorun yok; Log: "OpenGL ready: 4.6.0 NVIDIA 592.00 /
  RTX 5070 Laptop". Vulkan modu denendi mi bilinmiyor.
- 0.10 (2026-11-04): apt.dat okuyucu adım 1+2 (indeks, ayrıntı, Bezier,
  earcut, yüzey/sınır/boya çizgisi çizimi, GPU önbelleği). 334 test.
  Önizlemede KSEA doğru. Kullanıcı X-Plane'de denedi: "gayet güzel".
- 0.11 (2026-11-04): earcut third_party'ye, indeks önbelleği, havalimanı
  nesneleri + fare açıklaması. 341 test. **X-Plane'de henüz denenmedi.**
- scenery_packs.ini'ye ASLA yazılmaz (sadece okunur); X-Plane her açılışta
  kendisi yeniden yazar. Bu kurulumda Global Airports en üstte (X-Plane'in
  demo dosyası); KSEA zaten Global'de yok.

## Sıradaki
- **Karar (2026-11-04):** ekip motoru KULLANILMAYACAK (Qt bağımlılığı).
  apt.dat okuyucuyu biz saf C++ ile yazıyoruz: spesifikasyon XP-APT-1100
  (developer.x-plane.com, rev 16.11.2021; kullanıcı verdi). Dosya başlığı 1130.
  Tasarım: açılışta bayt-offset indeksi (diskte önbellek), ekrandaki
  havalimanları arka planda ayrıntılı ayrıştırılır (LRU). Custom Scenery
  apt.dat'ları Global'i ezer. Tüm öğeler (yüzey, çizgi, ışık, tabela, tulum,
  beacon, kalkış noktası...) gruplu açılır/kapanır katmanlarda.
- `GOREV_0.9.md` artık geçmiş kayıt (iş bilgisayarı planı iptal); A paketi
  burada yapıldı, B (motorla) iptal, C (modlar) parser'dan sonra.
- Harita modları: Scenery + VFR + IFR Low + IFR High (kullanıcı onayladı).
- Sıradaki: harita modları (Scenery / VFR / IFR Low / IFR High; tasarım
  GOREV_0.9.md bölüm C). Pist giriş çizgileri 1 piksel kalsın (kullanıcı
  bakacak). 0.11 ve 0.12 X-Plane'de henüz denenmedi.
- Açık sorular: zemin renkleri, önbellek disk sınırı, eski HaritaPlugin
  klasörünü silme.
