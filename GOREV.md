# GÖREV: X-Plane 11 için katmanlı, kendi penceresinde çalışan harita plugini

Bu dosya, sıfırdan başlayan bir Claude Code oturumunun bu işi baştan sona
yapabilmesi için yazıldı. Önceki araştırmanın sonuçları, kararlar ve dersler
aşağıda. Tamamını oku, sonra işe başla.

Kullanıcıyla **Türkçe** konuş. Kullanıcı X-Plane plugin geliştirmeye yeni, kodu
**anlayarak** ilerlemek istiyor: her aşamanın sonunda ne yaptığını ve neden
öyle yaptığını kısa ve sade bir dille anlat. Kod yorumları Türkçe olsun (ASCII,
yani ş/ğ/ı yerine s/g/i).

---

## 1. Amaç

X-Plane 11 içinde, **sadece haritadan oluşan** ve **katmanları tek tek
açılıp kapatılabilen** bir harita penceresi yapmak. Harita Kaşif, OSM ya da
başka bir dış harita veya karo servisi kullanmayacak. Bütün veri X-Plane'in
kendi dosyalarından ve SDK'sından gelecek. Böylece haritada görünen pist,
navaid ve kıyı şeridi simülatördekiyle birebir aynı olacak.

Beklenen yetenekler:
- Kaydırma (sürükleme) ve zoom (fare tekerleği, imlecin altındaki nokta sabit kalacak)
- Katmanlar: her biri ayrı ayrı açılıp kapatılabilir (liste Bölüm 5'te)
- Haritaya tıklayarak nokta, rota ve bölge işaretleme
- Uçağın konumu, uçağı takip etme, uçuş izi

## 2. Neden kendi penceremiz? (Alınmış karar, yeniden tartışma)

Önce X-Plane'in kendi haritası üzerine `XPLMMap` katmanı ekleyerek bir
prototip yapıldı (sürüm 0.2–0.3, bu klasörde). Çalıştı, ama iki ihtiyacı
karşılayamıyor:
1. X-Plane'in harita penceresinin yan bölmesi (Mode / Layers / Approach)
   SDK'dan kaldırılamıyor. Kullanıcı ekranda sadece haritayı istiyor.
2. X-Plane'in kendi katmanları (navaid, hava yolu vb.) SDK'dan
   açılıp kapatılamıyor. Sadece kendi eklediğimiz katman kontrol edilebiliyor.

Ek olarak harita katmanları fare olayı almıyor. Prototipte tıklama için
haritanın üstüne görünmez bir pencere bindirmek gerekti. Kendi penceremizde bu
sorun hiç yok, çünkü pencere tıklamayı zaten alıyor.

**Karar:** Harita, `XPLMCreateWindowEx` ile açılan kendi penceremizde, kendi
OpenGL çizimimizle yapılacak. `XPLMMap` API'si **kullanılmayacak**.

## 3. Ortam ve sabitler

| | |
|---|---|
| Simülatör | X-Plane 11.55r2, Windows 10/11 x64, `C:\X-Plane 11` |
| Proje klasörü | `C:\Projects\QT\prime_map_plugin` (bu klasör) |
| Derleyici | MSVC 2022 x64. Kullanıcının Qt Creator kiti "Desktop Qt 6.10.1 MSVC2022 64bit", generator Ninja. **Qt kullanılmıyor**; Qt Creator sadece IDE olarak kullanılıyor |
| SDK | X-Plane SDK başlıkları ve `XPLM_64.lib` projede: `third_party/XPSDK` (paket SDK 4.1.1; XP11 uyumu için sadece `XPLM200`, `XPLM210`, `XPLM300`, `XPLM301` tanımlı. XP12'ye özel API kullanılırsa derleme hata verir, bu istenen bir davranış) |
| Kurulum | Derleme sonrası CMake plugini `C:\X-Plane 11\Resources\plugins\HaritaPlugin\64\win.xpl` olarak kopyalıyor (`HARITA_DEPLOY` seçeneği) |
| Log | X-Plane `C:\X-Plane 11\Log.txt` dosyasına yazar; bizim satırlarımız `[HaritaPlugin]` ile başlar |

Klasördeki mevcut durum:
- `src/`, `tests/`, `resources/`, `CMakeLists.txt`, `README.md`, `CLAUDE.md`:
  XPLMMap prototipi (sürüm 0.3). Okuyup tanı.
- `prebuilt/`: prototipin hazır derlemesi.
- `_qt_template_yedek/`, `_to_delete/`, `build/`, `.qtcreator/`: kullanıcının
  dosyaları. **Bunlara dokunma.**

## 4. Prototipten alınan dersler (mutlaka uy)

1. **GL_CULL_FACE:** X-Plane bu ayarı açık bırakabiliyor. Açıkken dolu
   şekiller (GL_QUADS, GL_TRIANGLES, fan) **hiç görünmüyor**, sadece çizgiler
   görünüyor. Her çizimde kapat, çizim bitince eski haline döndür
   (`src/GLInclude.h` içindeki `CullFaceOff` sınıfı).
2. **Grafik durumu:** Çizimden önce `XPLMSetGraphicsState` çağır (fog 0,
   doku sayısı 0 veya 1, ışık 0, alpha test 0, blend 1, derinlik testi 0,
   derinlik yazma 0). Değiştirdiğin her GL durumunu (scissor, line width,
   stipple, cull) geri yükle.
3. **Doku:** Doku numarasını `XPLMGenerateTextureNumbers` ile al, bağlamak
   için `XPLMBindTexture2d` kullan (doğrudan `glBindTexture` değil).
4. **Yazı:** `XPLMDrawString` Türkçe karakterleri çizemez; arayüz metinleri
   ASCII olsun ("Bolge", "Katmanlar"). SDK sürümleri arasında `char*` /
   `const char*` farkı olduğu için SDK fonksiyonlarına her zaman değiştirilebilir
   bir `char` dizisi ver (prototipte bunun için kopya buffer kullanılıyor).
5. **Koordinatlar:** Modern pencerelerde (XPLM300) koordinatlar global
   masaüstü **boxel** biriminde. y yukarı doğru artar. UI ölçekleme %100 değilse
   boxel ≠ piksel. `glScissor` gibi piksel isteyen çağrılarda dönüşüm gerekir;
   pencere çizilirken OpenGL matrisinden boxel → piksel dönüşümü okunabilir
   (`src/ScreenState.cpp` bunu yapıyor).
6. **Thread:** XPLM fonksiyonları sadece ana thread'den çağrılır. Dosya
   okuma ve PNG çözme arka planda yapılabilir; doku yükleme ve XPLM çağrıları
   ana thread'de (flight loop veya draw callback içinde) yapılır.
7. **Pencere callback'leri:** `XPLMCreateWindowEx` için tüm callback'ler
   verilmeli; null bırakılmamalı.
8. **Tıklama geçirme:** Click callback'ten 0 döndürüp tıklamayı alttaki
   pencereye geçirmek SDK dokümanına göre fare takibini bozuyor; yapma.
   Tekerlek için 0 döndürmek sorun değil.
9. **Kilitli dosya:** X-Plane açıkken `win.xpl` kilitlidir, derleme sonrası
   kopyalama başarısız olur. Kullanıcıya X-Plane'i kapatmasını ya da plugini
   devre dışı bırakmasını söyle. Yeni plugin denemek için X-Plane'de
   **Developer → Reload plugins** kullanılabilir.
10. **Doğrulanmamış şeyleri doğrulanmış gibi sunma.** X-Plane'de denenmemiş
    her şeyi "denenecek" diye belirt.

## 5. Katmanlar ve veri kaynakları

Katmanlar alttan üste bu sırayla çizilir. Her biri ayrı bir aç/kapa
düğmesine sahip olacak. Açık/kapalı durumu plugin klasöründeki bir ayar
dosyasına (örneğin `harita_ayarlar.ini`) kaydedilip açılışta okunacak.

| # | Katman | Kaynak | Not |
|---|---|---|---|
| 1 | Zemin (su, kara, arazi türü) | Önceden üretilmiş PNG karolar (Bölüm 7) | Karo yoksa düz renk |
| 2 | Enlem/boylam ızgarası | Hesap | Zoom'a göre aralık |
| 3 | Pistler (ve sonra taksi yolları) | apt.dat (Bölüm 6) | Yakın zoom'da |
| 4 | Havalimanları | `XPLMNavigation` (`xplm_Nav_Airport`) | Zoom'a göre seyrelt |
| 5 | VOR / DME | `XPLMNavigation` | Ayrı düğme |
| 6 | NDB | `XPLMNavigation` | Ayrı düğme |
| 7 | ILS / localizer | `XPLMNavigation` | Ayrı düğme |
| 8 | Fix'ler | `XPLMNavigation` (`xplm_Nav_Fix`) | Çok fazla; sadece yakın zoom'da |
| 9 | Hava yolları (alçak / yüksek ayrı) | `earth_awy.dat` | Bölüm 6 |
| 10 | FMS rotası | `XPLMCountFMSEntries`, `XPLMGetFMSEntryInfo` | Her saniye yenile |
| 11 | Uçuş izi | Dataref'ler | Saniyede bir nokta |
| 12 | Uçak | `sim/flightmodel/position/latitude`, `longitude`, `psi` | En üstte |
| 13 | Kullanıcı çizimleri | `src/Drawings.*` (mevcut) | Nokta, rota, bölge |

Bu görevin kapsamı dışında, ama mimari buna izin vermeli: SID/STAR/yaklaşma
prosedürleri (CIFP dosyaları), AI trafik.

**Nav verisi nasıl yüklenecek:** Plugin açılınca (nav verisi hazır olduktan
sonra, örneğin ilk flight loop'larda veya `XPLM_MSG_AIRPORT_LOADED` gelince)
`XPLMGetFirstNavAid` / `XPLMGetNextNavAid` ile tüm kayıtları bir kez gez,
`XPLMGetNavAidInfo` ile tür, konum, kimlik, ad ve frekansı al. Kayıtları 1°×1°
hücrelik bir ızgaraya (spatial index) koy. Çizerken sadece ekranda görünen
hücrelere bak. On binlerce fix olduğu için bu şart.

## 6. Parser'lar

Sadece şu dosyalar için okuyucu yazılacak:

**a) apt.dat (pistler):** Kullanıcının ekibinin hazır bir apt.dat parse
motoru var, ama bu klasörde değil. **İşe başlarken kullanıcıya sor:** motoru
klasöre koyabilir mi, arayüzü nasıl? Gelene kadar:
- `IAirportGeometrySource` gibi bir arayüz tanımla (havalimanı ICAO → pist
  listesi: iki ucun lat/lon'u, genişlik, kimlik).
- Geçici olarak sadece kara pistlerini (satır kodu `100`) okuyan küçük bir
  okuyucu yaz. Dosyada açıkça "ekip motoru gelince değiştirilecek" diye işaretle.
- Okuma sırası: `Custom Scenery` paketleri `scenery_packs.ini` sırasıyla,
  sonra `Resources\default scenery\default apt dat\Earth nav data\apt.dat`.
  Aynı ICAO için önce gelen kazanır.
- Dosya büyük (yüz MB'larca); arka plan thread'inde oku, ana thread'i bloklama.

**b) earth_awy.dat (hava yolları):** Önce `C:\X-Plane 11\Custom Data\earth_awy.dat`,
yoksa `C:\X-Plane 11\Resources\default data\earth_awy.dat`. **Formatı tahmin
etme:** dosyanın başını oku, kullanıcının dosyasındaki gerçek satır biçimine
göre yaz. XP11 (1100) biçiminde her satır bir segment: iki uç noktanın kimliği,
bölge kodu ve türü, yön, seviye (alçak/yüksek), taban/tavan ve hava yolu
adları. Uç noktaların koordinatı bu dosyada yok: nav verisinden bul. Aynı
kimlikli birden çok nokta olabileceği için (kimlik + tür) eşleşen adaylar
arasından diğer uca en yakın olanı seç.

**c) DSF:** Plugin'in içinde değil, ayrı bir araçta (Bölüm 7).

## 7. Zemin: DSF → karo aracı

> **Not (0.8):** Bu bölüm uygulanmadı, yerine DSF okuyucu eklentinin içine
> yazıldı: zemin her bilgisayarda kurulu sahneyle (özel sahne dahil) kendiliğinden
> uyumlu kalsın diye (kullanıcı kararı). Ayrıntı: CLAUDE.md "Zemin kararı",
> README.md "Zemin nasıl çalışıyor". Aşağıdaki metin geçmiş kayıt olarak duruyor.

`tools/dsf2tiles/` altında, plugin'den bağımsız bir **Python** aracı.

Hazırlık:
- Laminar'ın komut satırı araçları (DSFTool dahil):
  https://files.x-plane.com/public/xptools/xptools_win_24-5.zip
  Kullanıcıdan indirip `tools/xptools/` altına açmasını iste veya yolu
  parametre olarak al. İndirme izni varsa kendin indirebilirsin.
- Dokümanlar:
  - https://developer.x-plane.com/article/dsf-usage-in-x-plane/
  - https://developer.x-plane.com/article/dsf-file-format-specification/
- DSF'ler: `C:\X-Plane 11\Global Scenery\X-Plane 11 Global Scenery\Earth nav data\<10° klasör>\<1° karo>.dsf`.
  Ad, karonun güney-batı köşesi (`+41+029` = enlem 41–42, boylam 29–30).
  XP11'de 7z ile sıkıştırılmış olabilir. DSFTool açamazsa önce `py7zr`
  ile çıkar.
- Bazı karo/klasörler yok: tamamen deniz olan karolar için DSF tutulmuyor, ya
  da o bölge kurulmamış. DSF'i olmayan karoyu **su** say, ama hangi karoların
  eksik olduğunu bir rapor dosyasına yaz.

Akış:
1. `DSFTool --dsf2text <dsf> <txt>` ile metne çevir (çıktıyı önbellekte tut).
2. **Önce gerçek bir çıktıyı incele.** Komut adlarını ve alan sırasını tahmin
   etme. Beklenen içerik kabaca: `PROPERTY sim/west|east|south|north`,
   `TERRAIN_DEF <ad>` listesi (sıra numarası = arazi türü indeksi),
   `BEGIN_PATCH <arazi indeksi> <yakın LOD> <uzak LOD> <bayrak> <köşe başına değer sayısı>`,
   `BEGIN_PRIMITIVE <0 üçgen | 1 şerit | 2 yelpaze>`,
   `PATCH_VERTEX <boylam> <enlem> <yükseklik> ...`, `END_PRIMITIVE`,
   `END_PATCH`.
3. Sadece asıl zemin patch'lerini al. Overlay patch'leri (bayrak & 2) atla,
   yoksa aynı yer iki kez boyanır.
4. Arazi türü adından renk seç (`terrain_Water` → su; adda `fst` → orman,
   `urb` → şehir, `crp` → tarla, vb.). Renk tablosu ayrı bir yapılandırma
   dosyasında olsun; kullanıcı renkleri oradan değiştirebilsin.
5. Üçgenleri Web Mercator karolarına (z/x/y, 256×256 PNG) boya. Varsayılan
   bölge Türkiye: enlem 35–43, boylam 25–45. Varsayılan zoom 5–11. İkisi de
   parametre olsun.
6. İsteğe bağlı kabartma (hillshade): köşe yüksekliklerinden. Yükseklik
   `-32768` ise değer ayrı raster'dan geliyor demek; o zaman ya raster'ı oku
   ya da kabartmayı atla ve bunu raporla.
7. Çıktı: `tiles/<z>/<x>/<y>.png`. Kurulum: `C:\X-Plane 11\Resources\plugins\HaritaPlugin\tiles\`.
8. İlerleme göster; yeniden çalıştırınca hazır karoları atla.

**Plugin tarafı:**
- PNG yükleme için `stb_image.h` (public domain, tek başlık) `third_party/` altına.
- Ekrandaki zoom'a göre uygun seviyedeki karoları çiz. Karo yoksa bir üst
  seviyedeki karonun ilgili parçasını büyüterek kullan, o da yoksa düz renk.
- Diskten okuma ve PNG çözme arka planda; `glTexImage2D` ana thread'de.
- Doku önbelleği LRU olsun (örneğin en fazla 256 doku).

## 8. Pencere ve arayüz

- **Pencere:** `XPLMCreateWindowEx`, `xplm_WindowDecorationRoundRectangle`
  (X-Plane'in ince çerçevesi; taşınabilir, boyutlandırılabilir),
  `xplm_WindowLayerFloatingWindows`. Pop-out desteği için
  `XPLMSetWindowPositioningMode` (pencere ayrı OS penceresine alınabilsin).
  Yer ve boyut kapanışta ayar dosyasına kaydedilsin.
- **İçerik:** Ekranın tamamı harita. Kontroller haritanın üstünde küçük ve
  yarı saydam:
  - Sol üstte "Katmanlar" düğmesi. Tıklanınca katman listesi (onay
    kutuları) açılır, tekrar tıklanınca kapanır. Varsayılan: kapalı.
  - Üstte ince bir araç şeridi: Gezin / Nokta / Rota / Bolge / Bitir /
    Geri al / Temizle, Ucagi takip et, Ucaga ortala.
  - Altta durum şeridi: imlecin lat/lon'u, zoom seviyesi, ölçek çubuğu (NM).
  - Bir düğmeyle tüm kontroller gizlenebilsin: sadece harita kalsın.
- **Etkileşim:**
  - Gezin modunda sol sürükleme kaydırır. Tekerlek her modda zoom yapar,
    imlecin altındaki nokta sabit kalır.
  - Nokta/Rota/Bolge modlarında sol tık köşe ekler, sağ tık bitirir.
  - Tıklama → lat/lon: kendi projeksiyonumuzun tersiyle (Web Mercator).
- **Projeksiyon:** Web Mercator; durum = merkez lat/lon + sürekli zoom değeri.
  Tüm çizim pencerenin boxel koordinatlarında; pencere alanıyla kırp
  (scissor, boxel → piksel dönüşümüyle).
- **Etiketler:** Zoom'a göre seyrelt (aynı yere üst üste binmesin);
  havalimanı ICAO, VOR/NDB kimlik ve frekans, fix adı, hava yolu adı.
- **Menü ve komutlar:** Plugins → Harita Plugin → "Haritayi ac / kapat",
  "Teshis modu". Komut: `ozdev/harita/toggle` (kullanıcı tuşa atayabilsin).

## 9. Kod yapısı

Mevcut prototipten şunlar **taşınacak**: `Drawings` (çizim modeli), `Geo`
(mesafe, büyük daire, üçgenleme), `Log`, menü/komut altyapısı, buton çizim
yaklaşımı, `GLInclude.h`. Şunlar **çıkarılacak**: `MapLayer`, `Overlay`,
`ClickWindow` ve XPLMMap'e özel matris hizalama kodu. Bunları silme;
`legacy/xplmmap_prototype/` altına taşı ve derlemeden çıkar.

Önerilen modüller (gerekirse değiştir, ama her dosyanın tek bir görevi olsun):

```
src/
  Plugin.cpp           giris noktalari, menu, komutlar, flight loop
  MapWindow.*          pencere, girdi (fare/tekerlek), cizim sirasi
  MapView.*            projeksiyon: merkez, zoom, lat/lon <-> boxel
  LayerManager.*       katman listesi, ac/kapa, ayarlarin kaydi
  layers/
    TileLayer.*        zemin karolari + doku onbellegi
    GridLayer.*
    AirportLayer.*     XPLMNavigation havalimanlari
    RunwayLayer.*      IAirportGeometrySource uzerinden
    NavaidLayer.*      VOR/NDB/ILS/fix (tur parametreli)
    AirwayLayer.*
    FmsLayer.*
    TrackLayer.*       ucus izi
    AircraftLayer.*
    DrawingLayer.*     kullanici cizimleri
  data/
    NavDatabase.*      tum nav kayitlari + 1x1 derece izgara
    AirwayReader.*
    AptDatReader.*     GECICI, ekip motoru gelince degisecek
  ui/
    Toolbar.*, LayerPanel.*, StatusBar.*
  Geo.*, Drawings.*, Log.*, GLInclude.h
tools/dsf2tiles/       Python araci + renk tablosu + README
tests/                 X-Plane'siz testler
```

Her katman ortak bir arayüzü uygulasın (ad, görünürlük, `Draw(ctx)`,
istenirse `DrawLabels(ctx)`). Yeni katman eklemek kolay olsun.

## 10. Çalışma planı ve kontrol noktaları

Aşamalarla ilerle. Her aşama sonunda **derle, testleri çalıştır, uyarıları
temizle**, `CLAUDE.md`'yi güncelle ve kullanıcıya ne yaptığını anlat. "X-Plane'de
dene" yazan noktalarda dur, kullanıcıya kısa bir test listesi ver ve sonucu
bekle. Log.txt'yi kendin okuyabiliyorsan oku.

1. **Hazırlık:** Mevcut kodu oku. Prototipi `legacy/` altına taşı. Yeni
   iskeleti kur, derlemenin çalıştığını göster. Kullanıcıya apt.dat motorunu
   sor.
2. **Harita motoru:** pencere, Web Mercator, kaydırma ve zoom, ızgara, uçak,
   katman altyapısı ve katman paneli, ayarların kaydı. **X-Plane'de dene.**
3. **Nav katmanları:** NavDatabase + ızgara, havalimanı, VOR, NDB, ILS, fix,
   etiket seyreltme. **X-Plane'de dene** (performans: fix katmanı açıkken FPS).
4. **Kullanıcı çizimleri:** nokta, rota, bölge, geri alma, temizleme; uçuş
   izi; FMS rotası. **X-Plane'de dene.**
5. **Pistler ve hava yolları:** apt.dat kaynağı (ekip motoru veya geçici
   okuyucu), earth_awy.dat. **X-Plane'de dene.**
6. **Zemin aracı:** önce tek karo (`+41+029`, İstanbul) ile sadece su/kara;
   PNG'leri kullanıcıya göster. Kıyı ve Boğaz düzgünse arazi türleri ve
   Türkiye geneli. Sonra plugin'de TileLayer. **X-Plane'de dene.**
7. **Bitiriş:** README (kurulum, kullanım, test listesi, bilinen sınırlar),
   CLAUDE.md, temiz derleme, testler.

## 11. Testler

`tests/` altında X-Plane'siz testler (CMake'te `HARITA_TESTS` seçeneği):
projeksiyon gidiş-dönüş (lat/lon → boxel → lat/lon), zoom'un imleç
etrafında yapılması, karo matematiği (lat/lon ↔ z/x/y), hava yolu satır
okuyucu, NavDatabase ızgara sorgusu, üçgenleme (mevcut testler korunacak).
Python aracı için de küçük testler yaz (patch okuma, rasterleştirme).

## 12. Yapma

- Dış harita/karo servisi, OSM, Kaşif, internetten harita verisi kullanma.
- XPLMMap API'sini kullanma.
- XP12'ye özel SDK fonksiyonu kullanma.
- Kullanıcının `_qt_template_yedek`, `_to_delete`, `build`, `.qtcreator`
  klasörlerine dokunma. X-Plane klasöründe sadece
  `Resources\plugins\HaritaPlugin\` altına yaz.
- Formatları tahmin etme: DSFTool çıktısının ve earth_awy.dat'ın gerçek
  satırlarına bakarak yaz.
- X-Plane'de denenmemiş bir şeyi "çalışıyor" diye raporlama.
