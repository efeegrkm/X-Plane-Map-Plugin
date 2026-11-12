# Zemin (DSF) yükleme hızı — araştırma raporu (2026-11-12)

Soru: X-Plane'de önbelleksiz ilk açılışta her DSF karosu 1–2 s sürüyor; tam sürümde
(binlerce karo) ilk harita makul sürede açılmıyor. Ne yapılabilir? Yüklenene kadar siyah
yerine kaba bir zemin gösterilebilir mi?

## 1. Süre nereye gidiyor

Ölçülmüş: 7z açma, üçgen geçişi, rakım sıkıştırma, karo toplamı, seviye 0 / 2 / 3 görüntüleri
(AGENTS.md "Süreler" ve 2026-11-12 önizleme çalışması). "(tahmin)" yazanlar ölçülmedi.

Tek işçi thread'i, düşük öncelikte, karoları **sırayla** üretir. Bir karo için:

| Aşama | Süre | Not |
|---|---|---|
| DSF'i diskten oku + 7z aç | 150–490 ms | LZMA tek thread; dosya boyuna bağlı |
| DSF başlık + üçgenleri sınıf raster'ına (4096²) boya | ~30 ms | |
| Vektörler (orman / yol) + overlay dışlamaları + alan tarama | 50–150 ms (tahmin) | ilk görüntü için gerekmiyor |
| Önbelleğe yaz (`.terrain`, `.vectors`) | 20–50 ms (tahmin) | |
| Rakım: piramit + **LZMA sıkıştırma** (`.elevation`) | 300–400 ms | karo üretiminin ~%40'ı |
| **Karo üretimi toplamı** | **0,7–1,0 s** | |
| İlk görüntü: seviye 0 (64 px) | ~26 ms | |
| İstenen görüntü: seviye 2 (1024 px) | ~330 ms | zoom 9–11'de karo başına 1 adet |
| Seviye 3 (zoom 11–13): 16 parça × ~340 ms | 5,4 s / karo | sadece ekrana giren parçalar |

Yani zoom ~10'da önbelleksiz bir karonun "tam" görünmesi **~1,1–1,4 s**; işçi düşük öncelikte
olduğu için X-Plane çekirdekleri doldurduğunda bu 2 s'ye çıkıyor. Ekranda 12 karo varsa ~15–25 s.
Önbellekten: raster + rakım okuma 60–75 ms + görüntü 330 ms ≈ **0,4 s / karo** (yine görüntü
üretimi baskın).

Ölçüm aracı artık kodda: **Diagnostics mode** açıkken Log.txt'ye `terrain profile stages`,
`terrain profile reloads`, `terrain profile waits` satırları yazılır (aşama başına adet × ortalama
ms, bellekten düşüp yeniden yüklenen karolar, isteklerin kuyrukta bekleme süresi). Aşağıdaki
önerilerin gerçek kazancı X-Plane'de bu satırlarla doğrulanmalı.

## 2. "Siyah yerine kaba zemin" — evet, üç kademede yapılabilir

| # | Yöntem | Ne zaman görünür | Maliyet | Durum |
|---|---|---|---|---|
| A | **Düz kara tonu yer tutucu**: sahnesi kurulu, dokusu hiç gelmemiş karo ortalama kara rengiyle doldurulur (deniz karoları zaten su rengi) | anında | yok | **0.23'te yapıldı** (Scenery modu) |
| B | **X-Plane'in kendi su şekilleri** (`Resources/map data/water/*.shp`, kurulu her karo için var, ESRI Polygon): kıyı çizgisi DSF açılmadan çizilir | 1–5 ms / karo | Küçük bir shapefile okuyucu + poligon tarama (~300 satır). Sadeleştirilmiş kıyı (medyan 10–15 m sapma), küçük göller yok; gerçek doku gelince yerini alır | öneri (en iyi görsel kazanç / emek) |
| C | **İki aşamalı karo üretimi**: 1. aşama sadece 7z aç + üçgenleri 1024² raster'a boya + DEM'in kaba seviyesi → seviye 0/1 görüntü; 2. aşama (vektör, tam raster, rakım sıkıştırma) arkadan | ~0,25–0,5 s / karo (bugün 0,8–1,0 s) | `BuildRaster`'ın ikiye bölünmesi; önbellek dosyaları 2. aşamada yazılır | öneri |
| D | **Kalıcı dünya özeti**: kurulu bütün karoların 64 px (seviye 0) görüntüsü tek bir atlas dosyasında; ilk kurulumdan sonra arka planda bir kez üretilir (karo başına ~0,2–0,3 s, sadece 1. aşama) | sonraki her açılışta anında, her yerde | Tam global sahnede ~18 bin karo ≈ 1–1,5 saat arka plan işi (bir kez; DSF değişince sadece o karo), dosya ~75 MB | öneri (tam sürüm için asıl çözüm) |

Katmanlı yükleme mantığı zaten var (seviye 0 → 2 → 3 → 4 → 5, kaba olan altta kalır); eksik olan
sadece "seviye 0'dan da önce" bir şey göstermekti (A) ve seviye 0'ın kendisinin pahalı karo
üretimini beklemesi (C, D).

## 3. Önbelleksiz hızı artırma

| # | Öneri | Beklenen kazanç | Risk / emek |
|---|---|---|---|
| 1 | **Karo üretimini paralelleştir** (2–4 thread'lik havuz, sadece `BuildRaster`: saf fonksiyon, kendi dosyalarını yazar; bellek LRU'ları yine tek işçide kalır) | Çekirdek sayısıyla orantılı: 3 thread ≈ 3× karo/saniye | Orta. Daha önce "ikinci işçi" LRU'lar kilitsiz olduğu için reddedilmişti; bu tasarım LRU'lara dokunmaz. Aynı karoyu iki thread üretmesin diye basit bir "üretiliyor" kümesi yeter |
| 2 | **Rakım sıkıştırmasını ertele**: `.elevation` önce ham (ya da hızlı delta + RLE) yazılır, LZMA işçi boştayken yapılır | Karo başına −0,3 s (~%35) | Düşük. Geçici dosya 2,9 MB yerine 0,75 MB olana kadar disk biraz büyür |
| 3 | **Vektörleri ertele**: orman / yol poligonları ilk görüntü için gerekmiyor; yol katmanı ya da alan bitleri istenince üretilsin | −0,05–0,15 s | Düşük (C ile birlikte gelir) |
| 4 | **İşçi önceliği**: ekran isteği beklerken `NORMAL`, arka plan (prefetch / precache) işinde `BELOW_NORMAL` | X-Plane yük altındayken 2 s → ~1 s | Çok düşük (2 satır); FPS etkisi Diagnostics ile ölçülmeli |
| 5 | **Önce seviye 1** (256 px, ~40 ms), sonra seviye 2: bugün seviye 0'dan doğrudan istenen seviyeye geçiliyor | İlk "okunur" görüntü 330 ms yerine ~70 ms | Düşük |
| 6 | Görüntü üretimini (ComposeImage) satır bantlarına bölüp paralel çalıştırma | Seviye 2/3 görüntüsü 330 → ~100 ms | Orta; 1. madde ile aynı havuz kullanılabilir |
| 7 | İlk kurulumda `Pre-cache` ile kullanıcının bölgesini önceden hazırlama (var) + D maddesi | İlk açılış deneyimi | Yok |

7z açma (150–490 ms) X-Plane'in dosya biçimi gereği kaçınılmaz; tek çare paralellik (1).

## 4. Önbellekten hızı artırma

| # | Öneri | Beklenen kazanç |
|---|---|---|
| 8 | **Hazır görüntüleri de önbellekle** (seviye 0–2, mod + palet başına; ham ya da hızlı sıkıştırılmış, karo başına ~0,3–1 MB): ikinci açılışta sınıf raster'ı + rakım okunup görüntü yeniden hesaplanmaz | 0,4 s → ~10–20 ms / karo (en büyük önbellekli kazanç) |
| 9 | Rakımın kaba seviyesini ayrı küçük dosyada tutma: uzak zoomda 0,75 MB LZMA açmak (~50 ms) yerine ~2 ms | Uzak zoomda karo başına −50 ms |
| 10 | Bellek listeleri: 0.23'te açık harita sayısıyla büyüyor (`prime_map.ini [memory]`); tek haritada da `growth` yerine tabanı büyütmek (raster 3 → 6) hızlı kaydırmada yeniden yüklemeyi azaltır | Log'daki `terrain profile reloads` satırı yüksekse |

## 5. Önerilen sıra

1. (yapıldı) A: yer tutucu kara tonu; profil satırları.
2. 4 + 5 + 2: küçük, düşük riskli; karo başına ~1,2 s → ~0,6 s ve ilk okunur görüntü çok daha erken.
3. 8: önbellekli açılış neredeyse anlık.
4. 1 (+6): çok çekirdekte 3× ve üstü.
5. B, sonra D: tam sürümde her yerde anında kaba harita.

Hepsi X-Plane'de denenmemiş tahminlerdir; her adımdan sonra Diagnostics satırlarıyla ölçülmeli.
