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
- C++17, CMake + MSVC 2022 (Qt Creator kiti "Desktop Qt 6.10.1 MSVC2022
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

## apt.dat kararı (kullanıcı, 2026-11-03)
- Ekibin apt.dat motoru iş bilgisayarında; güvenlik nedeniyle bize **hiçbir
  zaman** gelmeyecek. Kullanıcı işyerinde, izin verilen yerel bir AI ile
  bizim okuyucumuzun yerine ekip motorunu kendisi bağlayacak.
- Arayüz `data/AirportGeometry.h` (`IAirportGeometrySource`, başında bağlama
  talimatı), geçici `data/AptDatReader` (sadece 1/16/17 ve 100 satırları),
  bağlama noktası `Plugin.cpp` `CreateAirportGeometrySource()`. Katmanlar
  sadece `AirportGeometryStore`'u bilir. Arayüzü değiştirirsen talimatı,
  README'deki bölümü ve `FakeAirportSource` testini de güncelle.

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
- `gfx/Batch` (Line/Triangle/Point toplu çizim), `gfx/Texture`: OpenGL'in
  toplandığı yer. XP11 uyumluluk profili verdiği için hâlâ glBegin/glEnd;
  modernleştirme kararı kullanıcıya bırakıldı ("sen nasıl rahatsan").
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
- X-Plane'siz çekirdek testlere girer (`tests/tests.cpp`, 275 kontrol);
  `tools/preview` aynı katman kodunu XPLM taklitleriyle resme çizer.

## Doğrulanmış biçim / SDK gerçekleri
- SDK: ILS yönü GERÇEK yön (manyetik yok); TACAN frekansı kanal numarası
  (NUW = 85); "----" kimlikli yer tutucu ILS kayıtları var (filtreleniyor).
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

## Sıradaki
- **`GOREV_0.9.md`**: işyerindeki AI için ayrıntılı görev dosyası.
  A: OpenGL 3.3'e geçiş + C tarzı temizlik; B: havalimanı yüzeyleri (ekip
  motoru, depo DIŞINDAN bağlanır: motor kodu GitHub'a girmez); C: harita
  modları (Scenery / VFR / IFR Low / IFR High). Sıra A → B → C.
- Açık sorular: zemin renkleri, önbellek disk sınırı, eski HaritaPlugin
  klasörünü silme.
