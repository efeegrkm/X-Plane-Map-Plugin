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
- Dokunma: `_qt_template_yedek/`, `_to_delete/`, `build/`, `.qtcreator/`.

## apt.dat kararı (kullanıcı, 2026-11-03)
- Ekibin apt.dat motoru iş bilgisayarında; güvenlik nedeniyle bize **hiçbir
  zaman** gelmeyecek. Kullanıcı işyerinde, izin verilen yerel bir AI ile
  bizim okuyucumuzun yerine ekip motorunu kendisi bağlayacak.
- Bizim görevimiz (aşama 5): `IAirportGeometrySource` gibi dar bir arayüz +
  bunu uygulayan GEÇİCİ okuyucu (`AptDatReader`, X-Plane 11 apt.dat spec'ine
  göre). Değişim tek yerden olsun: arayüz, veri yapıları ve "motoru buraya
  bağla" noktası açık ve belgelenmiş; katmanlar sadece arayüzü bilsin.

## Mimari (sürüm 0.5)
- `legacy/xplmmap_prototype/`: eski XPLMMap prototipi (0.3), derlenmiyor.
- `src/Plugin.cpp`: giriş noktaları, menü, komutlar (`ozdev/harita/toggle`,
  `kontroller`, `teshis`), flight loop. Katman sırası `CreateLayers()`'ta.
- `MapWindow`: `XPLMCreateWindowEx` (RoundRectangle, FloatingWindows,
  pop-out). Sürükleme = kaydırma, tekerlek = imleç etrafında zoom.
  Çizim: zemin rengi -> katmanlar (geometri, sonra yazılar) -> kontroller.
  Scissor için boxel->piksel her çizimde GL matrisinden okunur (`GlScreen`).
- `MapView`: Web Mercator, merkez Mercator biriminde (0..1), sürekli zoom.
- `Layer` arayüzü + `LayerManager` (görünürlük `[katmanlar]` bölümüne kaydedilir).
- `layers/`: `GridLayer`, `AirportLayer`, `NavaidLayer` (Vor/Ndb/Ils/Fix,
  stil tablosu `kStyles`), `AircraftLayer`, ortak `MapSymbols`.
  `ui/`: `Toolbar`, `LayerPanel`, `StatusBar`, `UiDraw`.
- `data/AircraftData`: uçak dataref'leri. `data/NavDatabase`: kayıtlar +
  1x1 derece ızgara (CSR: mCellStart/mItems), DME eşleştirme.
  `data/NavLoader`: XPLMNavigation'ı flight loop'ta 4 ms'lik adımlarla gezer.
- `LabelPlacer`: etiket seyreltme; `DrawContext::mLabels` ortak, katman
  sırası = etiket önceliği. Katmanlar `Draw`'da görünen listeyi toplar,
  `DrawLabels` aynı listeyi kullanır.
- X-Plane'siz çekirdek (testlere girer): `Geo`, `Drawings`, `ScreenMath`,
  `MapView`, `MapFormat`, `Settings`, `LayerManager`, `LabelPlacer`,
  `NavDatabase`.
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
- 0.5 (aşama 3): nav katmanları yazıldı. /W4 uyarısız, 166 test geçiyor,
  X-Plane'e kuruldu. **X-Plane'de henüz denenmedi.** Gerçek veriyle ölçüm:
  255 bin kayıt, `Build` 10 ms; zoom 9'da en yoğun fix sorgusu ~1900 kayıt.
- Doğrulanacak: SDK'nın ILS yönünü ham mı (manyetik*360+gerçek) yoksa
  çözülmüş mü verdiği (Log'daki `ILS ornegi` satırları); fix açıkken FPS.

## Sıradaki
1. [ ] Kullanıcı README'deki aşama 3 test listesini uygulasın, Log'u oku
2. [ ] Aşama 4: kullanıcı çizimleri (Drawings + DrawingLayer, araç
       şeridine Gezin/Nokta/Rota/Bolge/Bitir/Geri al/Temizle), uçuş izi, FMS
3. [ ] Aşama 5: pistler (arayüz + geçici apt.dat okuyucu, yukarıdaki karar),
       earth_awy.dat hava yolları (önce dosyanın gerçek satırlarına bak)
