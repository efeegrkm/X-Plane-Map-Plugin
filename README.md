# Harita Plugin 0.4 — X-Plane 11

X-Plane içinde, **kendi penceresinde** çalışan, katmanları tek tek açılıp
kapatılabilen harita (AR-GE). Dış harita servisi (Kaşif, OSM) yok; tüm veri
X-Plane'in kendi dosyalarından ve SDK'sından gelir.

Sürüm 0.4 = GÖREV.md aşama 1 + 2: pencere, Web Mercator, kaydırma/zoom,
enlem/boylam ızgarası, uçak, katman paneli, ayarların kaydı. Navaid, pist,
hava yolu, çizim ve zemin katmanları sonraki aşamalarda gelecek.

Eski XPLMMap prototipi (0.3): `legacy/xplmmap_prototype/` (derlenmiyor).

## Derleme ve kurulum

Qt Creator'da projeyi aç ve derle (kit: Desktop Qt 6.10.1 MSVC2022 64bit).
Derleme bitince CMake eklentiyi kendiliğinden kurar:

```
C:\X-Plane 11\Resources\plugins\HaritaPlugin\64\win.xpl
```

- X-Plane açıkken `win.xpl` kilitlidir, kopyalama başarısız olur. Önce
  X-Plane'i kapat ya da Plugin Admin'den eklentiyi devre dışı bırak.
- Farklı X-Plane klasörü: `-DXPLANE_DIR=...`; kurulumu kapatmak: `-DHARITA_DEPLOY=OFF`.
- X-Plane'siz testler: `-DHARITA_TESTS=ON`, sonra `harita_tests.exe` çalıştır.

Log: `C:\X-Plane 11\Log.txt` içinde `[HaritaPlugin]` ile başlayan satırlar.

## Kullanım

- Aç / kapat: **Plugins → Harita Plugin → Haritayi ac / kapat** veya
  `ozdev/harita/toggle` komutu (Settings → Keyboard'da tuşa atanabilir).
- **Sürükle**: haritayı kaydırır (uçak takibini kapatır).
- **Tekerlek**: zoom; imlecin altındaki nokta yerinde kalır. Uçak takibi
  açıkken zoom uçağın etrafında yapılır.
- Üst şerit:

| Düğme | Ne yapar |
|---|---|
| Katmanlar | Katman listesini açar/kapatır (onay kutuları) |
| Ucagi takip et | Harita uçakla birlikte kayar |
| Ucaga ortala | Haritayı bir kez uçağa ortalar |
| Disari al / Iceri al | Pencereyi ayrı işletim sistemi penceresine alır / geri koyar |
| Gizle | Tüm kontrolleri gizler; imleç haritaya gelince sol üstte **Kontroller** düğmesi çıkar |

- Alt şerit: imlecin altındaki enlem/boylam, zoom seviyesi, ölçek çubuğu (NM;
  haritanın merkez enlemine göre).
- Pencere yeri/boyutu, merkez, zoom, takip ve katmanların açık/kapalı durumu
  `HaritaPlugin\harita_ayarlar.ini` dosyasına kaydedilir.
- **Teshis modu** (menü veya `ozdev/harita/teshis`): Log.txt'ye 2 saniyede
  bir `TESHIS` satırı yazar (pencere, UI ölçek, kırpma kutusu, imleç).

## Test listesi (aşama 2) — X-Plane'de denenecek

| # | Ne yap | Beklenen |
|---|---|---|
| 1 | X-Plane'i aç, uçuş başlat | Log'da `yuklendi (surum 0.4)` ve `harita penceresi olusturuldu` |
| 2 | Plugins → Harita Plugin → Haritayi ac / kapat | Ekran ortasında "Harita" başlıklı pencere; ızgara ve sarı uçak sembolü |
| 3 | Uçak sembolü | Uçağın konumunda, burnu uçağın yönünde (X-Plane'in kendi haritasıyla karşılaştır) |
| 4 | Haritayı sürükle | Harita fareyle birlikte kayar, "Ucagi takip et" sönümlenir |
| 5 | Tekerlek ile zoom | İmlecin altındaki ızgara kesişimi yerinde kalır; alt şeritte zoom değişir |
| 6 | Ucagi takip et / Ucaga ortala | Harita uçağa döner |
| 7 | Katmanlar → kutulara tıkla | Izgara / uçak açılıp kapanır |
| 8 | Pencereyi taşı, boyutlandır | Harita pencereyle birlikte; çizimler pencere dışına taşmaz |
| 9 | Gizle | Şeritler kaybolur; imleç haritadayken sol üstte "Kontroller" |
| 10 | Disari al, sonra Iceri al | Ayrı pencere olur; çizim, sürükleme, zoom çalışıyor mu? Geri alınca eski yerine döner mü? |
| 11 | X-Plane'i kapat, tekrar aç | Pencere aynı yerde/boyutta, aynı zoom ve katmanlarla açılır |
| 12 | Settings → Graphics → UI ölçek %100 ve %150 | 4, 5, 8'i ikisinde de tekrarla; çizim pencere kenarında doğru kesiliyor mu |
| 13 | Teshis modu aç, 10 sn bekle | Log.txt'de `TESHIS` satırları |

Sorun olursa teşhis modunu açıp aynı şeyi tekrarla ve `TESHIS` satırlarını gönder.

## Dosyalar

| Dosya | Görev |
|---|---|
| `src/Plugin.cpp` | Giriş noktaları, menü, komutlar, flight loop, katman sırası |
| `src/MapWindow.*` | Pencere, fare/tekerlek, çizim sırası, ayar kaydı |
| `src/MapView.*` | Web Mercator projeksiyonu: merkez, zoom, lat/lon ↔ boxel |
| `src/Layer.h`, `src/LayerManager.*` | Katman arayüzü, katman listesi, görünürlük kaydı |
| `src/layers/GridLayer.*` | Enlem/boylam ızgarası |
| `src/layers/AircraftLayer.*` | Uçak sembolü |
| `src/data/AircraftData.*` | Uçak konumu/yönü (dataref) |
| `src/ui/Toolbar.*`, `LayerPanel.*`, `StatusBar.*` | Üst şerit, katman paneli, alt şerit |
| `src/ui/UiDraw.*` | Kutu, yazı, düğme, onay kutusu çizimi |
| `src/GlScreen.*` | boxel → piksel dönüşümü, pencereye kırpma (scissor) |
| `src/Settings.*` | INI ayar dosyası |
| `src/MapFormat.*` | Koordinat yazısı, ızgara aralığı, ölçek çubuğu seçimi |
| `src/Geo.*`, `src/Drawings.*` | Mesafe/büyük daire/üçgenleme, çizim modeli (aşama 4'te kullanılacak) |
| `tests/tests.cpp` | X-Plane'siz testler |

## Bilinen sınırlar

- Zemin henüz düz renk (DSF karoları aşama 6).
- Ölçek çubuğu merkez enlemine göre; Mercator'da ölçek enlemle değişir.
- 180. boylam civarı sadece kısmen ele alınıyor (çizgiler en yakın kopyaya göre).
- Pop-out penceresinde imleç konumu cursor callback'ten okunuyor; X-Plane'de denenecek.
