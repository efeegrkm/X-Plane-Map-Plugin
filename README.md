# Harita Plugin 0.5 — X-Plane 11

X-Plane içinde, **kendi penceresinde** çalışan, katmanları tek tek açılıp
kapatılabilen harita (AR-GE). Dış harita servisi (Kaşif, OSM) yok; tüm veri
X-Plane'in kendi dosyalarından ve SDK'sından gelir.

- 0.4 = GÖREV.md aşama 1 + 2: pencere, Web Mercator, kaydırma/zoom, ızgara,
  uçak, katman paneli, ayarların kaydı (X-Plane'de denendi, sorun bildirilmedi).
- 0.5 = aşama 3: X-Plane'in nav verisinden havalimanı, VOR/DME, NDB, ILS/LOC,
  fix katmanları ve etiket seyreltme (**X-Plane'de henüz denenmedi**).

Pist, hava yolu, çizim, uçuş izi ve zemin katmanları sonraki aşamalarda.

## Katmanlar

Alttan üste çizim sırası. Etiketler de bu sırayla yer bulur (üstteki satırın
etiketi önceliklidir); çakışan etiket çizilmez, zoom yapınca görünür.

| Katman | Sembol | Görünür | Etiket | Ayrıntı |
|---|---|---|---|---|
| Enlem/boylam ızgarası | ince çizgi | her zoom | kenarlarda | aralık zoom'a göre |
| Havalimanları | mavi halka (büyük: ortası dolu) | zoom 4 (küçükler 8) | 6 (küçükler 9) | zoom 11: ICAO + ad |
| VOR / DME | altıgen; VOR/DME: altıgen + kare; DME: kare | 5 | 6.5 | zoom 8: frekans |
| NDB | noktalı çember | 7 | 8 | zoom 9: frekans (kHz) |
| ILS / LOC | 10 NM'lik "tüy" (yaklaşma yönü) | 8.5 | 10 | frekans + manyetik yön |
| Fix'ler (varsayılan kapalı) | küçük üçgen | 9 | 10 | — |
| Uçak | sarı uçak | her zoom | — | — |

- "Büyük havalimanı" şimdilik 4 harfli, rakamsız ICAO kodu demek (LTFM evet,
  LT51 hayır). Pist verisi gelince pist uzunluğuna göre iyileşecek.
- Nav verisi açılıştan sonra birkaç saniyede, karelere bölünerek okunur
  (alt şeritte "Nav verisi yukleniyor").

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
- **Teshis modu** (menü veya `ozdev/harita/teshis`): alt şeritte FPS ve
  haritanın kendi çizim süresi (ms); Log.txt'ye 2 saniyede bir `TESHIS`
  satırı (pencere, UI ölçek, kırpma kutusu, imleç, etiket sayısı, FPS).

## Test listesi (aşama 3) — X-Plane'de denenecek

| # | Ne yap | Beklenen |
|---|---|---|
| 1 | X-Plane'i aç, uçuş başlat, haritayı aç | Birkaç saniye alt şeritte "Nav verisi yukleniyor: ..." sonra kaybolur. Log'da `nav verisi hazir: ... kayit` ve 3 satır `ILS ornegi` |
| 2 | Türkiye, zoom 5-6 | Büyük havalimanları mavi halka + ICAO; VOR'lar altıgen; yazılar üst üste binmiyor |
| 3 | Zoom 8-9 (örnek: LTAC çevresi) | Küçük havalimanları, NDB'ler ve VOR frekansları da görünür |
| 4 | X-Plane'in kendi haritasıyla karşılaştır | Havalimanı ve navaid'ler aynı yerde mi (aynı veri olmalı) |
| 5 | LTAC, zoom 9-10 | Pistlerin ucundan dışarı uzanan turuncu ILS tüyleri; yönleri pistlerle aynı mı; etikette frekans ve yön |
| 6 | Katmanlar → Fix'ler'i aç, zoom 9-11, yoğun bir bölge (Frankfurt/Londra) | Küçük üçgenler + adları |
| 7 | Teshis modunu aç, fix açık ve kapalı iken FPS ve "cizim ... ms" değerine bak | Değerleri not et (performans testi) |
| 8 | Haritayı hızlıca kaydır / zoom yap | Takılma yok mu |
| 9 | Katmanları kapat, X-Plane'i yeniden aç | Aynı katmanlar kapalı gelir |

Log.txt'deki `nav verisi hazir` ve `ILS ornegi` satırlarını bana gönder (veya
bana Log.txt'yi okut): ILS yön kodlamasının doğru çözüldüğünü bunlarla doğrulayacağım.

## Test listesi (aşama 2) — denendi, sorun bildirilmedi

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
| `src/layers/AirportLayer.*` | Havalimanları (seyreltmeli) |
| `src/layers/NavaidLayer.*` | VOR/DME, NDB, ILS/LOC, fix (tür parametreli) |
| `src/layers/MapSymbols.*` | Ortak sembol çizimi, sembol yanına etiket |
| `src/layers/AircraftLayer.*` | Uçak sembolü |
| `src/data/AircraftData.*` | Uçak konumu/yönü (dataref) |
| `src/data/NavDatabase.*` | Nav kayıtları + 1°×1° ızgara, DME eşleştirme |
| `src/data/NavLoader.*` | X-Plane'in nav verisini SDK ile karelere bölerek okur |
| `src/LabelPlacer.*` | Etiket/sembol çakışma kontrolü (seyreltme) |
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
- Pop-out penceresinde imleç konumu cursor callback'ten okunuyor.
- ILS tüyü localizer anteninden çizilir (pistin öbür ucu); pist verisi gelince
  eşik noktasından çizilebilir.
- VOR'larda manyetik sapma (pusula gülü) çizilmiyor.
