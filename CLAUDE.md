# X-Plane Harita Plugini — Proje Notu

## Amaç
X-Plane 11 içinde, Kaşif / OSM / dış harita servisi kullanmadan, sadece
X-Plane'in kendi verisi + OpenGL ile, **kendi penceresinde** çalışan ve
katmanları tek tek açılıp kapatılabilen bir harita (AR-GE). Ayrıntılı görev
tanımı, kararlar ve aşamalar: **`GOREV.md`** (önce onu oku).

## Çalışma şekli
- Kullanıcıyla Türkçe konuş. Kullanıcı X-Plane plugin geliştirmeye yeni,
  kodu ANLAYARAK ilerlemek istiyor: her aşama sonunda ne yapıldığını ve
  nedenini sade anlat.
- GOREV.md Bölüm 10'daki aşamalarla ilerle. Her aşama sonunda: derle,
  testleri çalıştır, uyarıları temizle, bu dosyayı güncelle. "X-Plane'de
  dene" noktalarında dur, test listesi ver, sonucu bekle.
- X-Plane'de denenmemiş hiçbir şeyi "çalışıyor" diye raporlama.

## Kod standardı (kullanıcının isteği, mutlaka uy)
- Önekler: `m` üye değişken (`mZoom`), `p` parametre (`pZoom`), `t` yerel /
  geçici değişken (`tZoom`, döngü sayaçları dahil: `tIndex`), `k` sabit
  (`kMaxZoom`), `g` global (tek global: `gPlugin`). Yapı (struct) alanları da
  `m` önekli (`LatLon::mLat`).
- Süslü parantez her zaman **alt satırda** açılır (sınıf, fonksiyon,
  namespace, if/else, döngü, switch). Tek satırlık `if` bile parantezli.
- Tür ve fonksiyon adları PascalCase (`MapView::ZoomAt`).
- Kod yorumları Türkçe, ASCII (ş/ğ/ı yerine s/g/i). Arayüz metinleri ASCII
  (`XPLMDrawString` Türkçe harf çizemez).
- XPLM callback'lerine refcon olarak `this` verilir; global durum yok.

## Sabitler
- X-Plane 11.55r2, Windows x64, `C:\X-Plane 11`. SDK projede:
  `third_party/XPSDK` (sadece XPLM200..XPLM301 tanımlı; XP12 API'si derlenmez).
- C++17, CMake + MSVC 2022 (Qt Creator kiti "Desktop Qt 6.10.1 MSVC2022
  64bit", Ninja; Qt kütüphanesi kullanılmıyor). Uyarı seviyesi /W4.
- Komut satırından derleme: vcvars64 + `C:\Qt\Tools\CMake_64\bin\cmake.exe`
  + `C:\Qt\Tools\Ninja\ninja.exe`. Kullanıcının `build/` klasörüne dokunma;
  kendi derlemeni scratchpad'de yap.
- Kurulum: CMake (`HARITA_DEPLOY=ON`) `win.xpl`'yi
  `C:\X-Plane 11\Resources\plugins\HaritaPlugin\64\` altına kopyalar.
  X-Plane açıkken dosya kilitli olur.
- Ayar dosyası: `...\plugins\HaritaPlugin\harita_ayarlar.ini`.
- DSF (2026-11-03 kontrol edildi): kurulu sahne SADECE
  `Global Scenery/X-Plane 11 Demo Areas` (35 karo: Hawaii +20-160 klasörü,
  Seattle +46..+48 / -121..-124, Juneau +57..+59 / -133..-137). Türkiye
  (+41+029) YOK. DSF'ler 7z sıkıştırılmış (imza `7z\xBC\xAF\x27\x1C`).
  Aşama 6'ya başlamadan kullanıcıya sor: Türkiye sahnesi kurulacak mı, yoksa
  Seattle demo bölgesiyle mi ilerlenecek.
- Dokunma: `_qt_template_yedek/`, `_to_delete/`, `build/`, `.qtcreator/`.

## apt.dat kararı (kullanıcı, 2026-11-03)
- Ekibin apt.dat motoru iş bilgisayarında; güvenlik nedeniyle bize **hiçbir
  zaman** gelmeyecek. Kullanıcı işyerinde, izin verilen yerel bir AI ile
  bizim okuyucumuzun yerine ekip motorunu kendisi bağlayacak.
- Yapıldı (0.6): arayüz `data/AirportGeometry.h` (`IAirportGeometrySource`,
  başında bağlama talimatı), geçici `data/AptDatReader` (sadece 1/16/17 ve
  100 satırları), bağlama noktası `Plugin.cpp` `CreateAirportGeometrySource()`.
  Katmanlar sadece `AirportGeometryStore`'u bilir. Arayüzü değiştirirsen
  talimatı ve `FakeAirportSource` testini de güncelle.

## Mimari (sürüm 0.6)
- `legacy/xplmmap_prototype/`: eski XPLMMap prototipi (0.3), derlenmiyor.
- `src/Plugin.cpp`: giriş noktaları, menü, komutlar (`ozdev/harita/toggle`,
  `kontroller`, `teshis`), flight loop. Katman sırası `CreateLayers()`'ta.
- `MapWindow`: `XPLMCreateWindowEx` (RoundRectangle, FloatingWindows,
  pop-out). Sürükleme = kaydırma, tekerlek = imleç etrafında zoom.
  Çizim: zemin rengi -> katmanlar (geometri, sonra yazılar) -> kontroller.
  Scissor için boxel->piksel her çizimde GL matrisinden okunur (`GlScreen`).
- `MapView`: Web Mercator, merkez Mercator biriminde (0..1), sürekli zoom.
- `Layer` arayüzü + `LayerManager` (görünürlük `[katmanlar]` bölümüne kaydedilir).
- `layers/`: `GridLayer`, `RunwayLayer`, `AirportLayer`, `NavaidLayer`
  (Vor/Ndb/Ils/Fix, stil tablosu `kStyles`), `AircraftLayer`, ortak `MapSymbols`.
  `ui/`: `Toolbar`, `LayerPanel`, `StatusBar`, `UiDraw`.
- `GeoGrid`: 1x1 derece CSR ızgara (nav ve pist verisi ortak kullanır).
- `data/AircraftData`: uçak dataref'leri. `data/NavDatabase`: kayıtlar +
  tür başına `GeoGrid`, DME eşleştirme. `data/NavLoader`: XPLMNavigation'ı
  flight loop'ta 4 ms'lik adımlarla gezer (gerçekte 48 ms'de bitiyor).
- `data/AirportGeometryStore`: kaynağı `std::thread`'de çalıştırır, `Poll()`
  ile ana thread'e alır; arka plan thread'inde XPLM/Log YOK (rapor metni döner).
- `LabelPlacer`: etiket seyreltme; `DrawContext::mLabels` ortak, katman
  sırası = etiket önceliği. Katmanlar `Draw`'da sembol kutularını `Reserve`
  eder (etiketler sembolleri örtmez), `DrawLabels` aynı listeyi kullanır.
- X-Plane'siz çekirdek (testlere girer): `Geo`, `Drawings`, `ScreenMath`,
  `MapView`, `MapFormat`, `Settings`, `LayerManager`, `LabelPlacer`,
  `GeoGrid`, `NavDatabase`, `AirportGeometry*`, `AptDatReader`.
- SDK gerçekleri (X-Plane 11.55'te görüldü): ILS yönü çözülmüş GERÇEK yön
  (manyetik yok); TACAN frekansı kanal numarası (NUW = 85); "----" kimlikli
  yer tutucu ILS kayıtları var (filtreleniyor).
- UI görünümü (koyu, yarı saydam şeritler, `Ui::k...` renkleri) kullanıcı
  tarafından beğenildi; yeni arayüz parçaları aynı paletle yapılmalı.

## Prototipten dersler (GOREV.md Bölüm 4)
- GL_CULL_FACE kapat (`CullFaceOff`), `XPLMSetGraphicsState` her geometri
  çiziminden önce (`Ui::UseFlatState`), çünkü `XPLMDrawString` doku açıyor.
- Tıklamayı alttaki pencereye geçirme (fare takibi bozuluyor).

## Durum
- 0.4 (aşama 1+2): kullanıcı X-Plane'de denedi, beğendi; log'da katman
  aç/kapa ve pop-out var, hata yok. Pop-out'ta imleç/scissor ayrıca
  doğrulanmadı.
- 0.5 (aşama 3): X-Plane'de denendi (ekran görüntüleri): semboller doğru
  yerde, ILS yönleri pistlerle uyumlu. Görülen kusurlar 0.6'da düzeltildi
  (etiket-sembol çakışması, TACAN "0.85", ince tüy, "----" kayıtları).
  FPS ölçümü henüz yapılmadı.
- 0.6: aşama 3 düzeltmeleri + pistler (aşama 5a, kullanıcının önceliği
  "havalimanı poligonu" olduğu için aşama 4'ten önce). /W4 uyarısız, 198 test,
  X-Plane'e kuruldu. Geçici okuyucu gerçek veride: 29.895 havalimanı, 0.4 sn
  (sıcak önbellek). **X-Plane'de henüz denenmedi.**

## Sıradaki
1. [ ] Kullanıcı README'deki 0.6 test listesini uygulasın, Log'u oku
2. [ ] Aşama 4: kullanıcı çizimleri (Drawings + DrawingLayer, araç
       şeridine Gezin/Nokta/Rota/Bolge/Bitir/Geri al/Temizle), uçuş izi, FMS
3. [ ] Aşama 5b: earth_awy.dat hava yolları (önce dosyanın gerçek satırlarına bak)
4. [ ] Aşama 6: DSF -> karo aracı (zemin)
5. [ ] Fikir: pist verisi varken ILS tüyünü eşikten çizmek; havalimanı
       önemini pist uzunluğuyla belirlemek; taksi yolları (ekip motoru verirse)
