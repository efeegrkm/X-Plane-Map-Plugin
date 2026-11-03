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
- Ekipte hazır bir apt.dat parse motoru var; yeniden yazılmayacak
  (kullanıcıya soruldu, cevap bekleniyor).

## Mimari (sürüm 0.4)
- `legacy/xplmmap_prototype/`: eski XPLMMap prototipi (0.3), derlenmiyor.
- `src/Plugin.cpp`: giriş noktaları, menü, komutlar (`ozdev/harita/toggle`,
  `kontroller`, `teshis`), flight loop. Katman sırası `CreateLayers()`'ta.
- `MapWindow`: `XPLMCreateWindowEx` (RoundRectangle, FloatingWindows,
  pop-out). Sürükleme = kaydırma, tekerlek = imleç etrafında zoom.
  Çizim: zemin rengi -> katmanlar (geometri, sonra yazılar) -> kontroller.
  Scissor için boxel->piksel her çizimde GL matrisinden okunur (`GlScreen`).
- `MapView`: Web Mercator, merkez Mercator biriminde (0..1), sürekli zoom.
- `Layer` arayüzü + `LayerManager` (görünürlük `[katmanlar]` bölümüne kaydedilir).
- `layers/`: `GridLayer`, `AircraftLayer`. `ui/`: `Toolbar`, `LayerPanel`,
  `StatusBar`, `UiDraw`. `data/AircraftData`: uçak dataref'leri.
- X-Plane'siz çekirdek (testlere girer): `Geo`, `Drawings`, `ScreenMath`,
  `MapView`, `MapFormat`, `Settings`, `LayerManager`.

## Prototipten dersler (GOREV.md Bölüm 4)
- GL_CULL_FACE kapat (`CullFaceOff`), `XPLMSetGraphicsState` her geometri
  çiziminden önce (`Ui::UseFlatState`), çünkü `XPLMDrawString` doku açıyor.
- Tıklamayı alttaki pencereye geçirme (fare takibi bozuluyor).

## Durum
- Aşama 1 (hazırlık) ve aşama 2 (harita motoru) yazıldı: sürüm 0.4.
  MSVC /W4 uyarısız derleniyor, 132 test geçiyor, X-Plane'e kuruldu.
  **X-Plane'de henüz denenmedi.**
- Denenecek belirsizlikler: pop-out penceresinde çizim/scissor/imleç;
  cursor callback'in her karede gelip gelmediği; UI ölçek %150'de kırpma.

## Sıradaki
1. [ ] Kullanıcı README'deki aşama 2 test listesini X-Plane'de uygulasın
2. [ ] Kullanıcıdan apt.dat motoru cevabı (klasöre konabilir mi, arayüzü?)
3. [ ] Aşama 3: NavDatabase (1x1 derece ızgara) + havalimanı/VOR/NDB/ILS/fix
       katmanları + etiket seyreltme
