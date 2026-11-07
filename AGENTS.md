# AGENTS.md — Yapay zekâ asistanları için proje notu

Bu dosya, bu projede çalışacak her yapay zekâ asistanının (Claude Code, Codex,
Cursor, Copilot ...) **sıfırdan, başka hiçbir sohbet geçmişi olmadan** işe
başlayabilmesi için yazıldı. Tamamını oku. Sonra:

- **`MIMARI.pdf`** (kaynağı `docs/MIMARI.html`): kodun mimarisi, veri akışı
  şemaları, her dosyanın görevi. Koda girmeden önce oku.
- **`README.md`**: kullanıcı gözünden kullanım, katmanlar, ayar dosyaları,
  X-Plane test listesi.
- Her kaynak dosyanın başında Türkçe bir açıklama bloğu var (ne yapar, neden,
  zoom eşikleri, küçük şemalar). Bir dosyayı değiştirmeden önce o bloğu oku.

Durum tarihi: **2026-11-07**, sürüm **0.18**.

---

## 1. Proje

X-Plane 11 için bir harita eklentisi (`win.xpl`, C++20, Windows x64). Harita
X-Plane'in içinde **kendi penceresinde** çizilir; katmanlar gruplu ve tek tek
(alt katmanlara kadar) açılıp kapanır; dört harita modu var: Scenery, VFR,
IFR Low, IFR High, Dark (0.18).

**Veri sadece X-Plane'den gelir** (dış harita servisi, OSM, Kaşif, internet
YOK):
- zemin: o bilgisayarda kurulu DSF sahne dosyaları,
- havalimanları: apt.dat (kendi okuyucumuz),
- navaid'ler: X-Plane SDK'sının nav veritabanı,
- hava yolları: `earth_awy.dat`,
- uçak / FMS: dataref'ler ve FMS API'si.

**Asıl kullanım:** kullanıcının simülatörlerindeki **eğitmen istasyonu (IOS)
ekranları**. Özellik önerirken eğitmen iş akışını düşün (uçağı konumlandırma,
trafik, debrief, büyük ekranda okunurluk).

### Alınmış temel kararlar (yeniden tartışma)
| Karar | Neden |
|---|---|
| Kendi pencere (`XPLMCreateWindowEx`), `XPLMMap` API'si **kullanılmaz** | X-Plane harita penceresinin yan bölmesi kaldırılamıyor, X-Plane'in kendi katmanları kapatılamıyor, harita katmanları fare olayı almıyor (ilk prototipte denendi) |
| apt.dat okuyucu projenin kendisi (saf C++), ekibin Qt tabanlı motoru **kullanılmaz** | Qt bağımlılığı, yavaş veritabanı katmanı. Kaynak: XP-APT-1100 spesifikasyonu (rev 16.11.2021) |
| Zemin, eklentinin içinde DSF'ten üretilir (çevrimdışı karo aracı yok) | Her bilgisayarda kurulu **her** sahneyle çalışmalı; özel sahne global'i ezmeli; sahne güncellenince harita kendiliğinden güncellenmeli |
| Modern OpenGL 3.3 (VBO/VAO/GLSL), GL çağrısı sadece `src/gfx/` | X-Plane 11 uyumluluk profili veriyor ama glBegin/glEnd terk edildi |
| Üçgen disk önbelleği yok (apt.dat yüzeyleri) | En kötü havalimanı ~0,6 s; gerek görülmedi |
| Harita modları: Scenery + VFR + IFR Low + IFR High + Dark | Kullanıcı onayladı (Dark 0.18: siyah minimal) |
| Eklenti uçağın konumunu **değiştirmez**; "Go to airport" sadece haritayı taşır | Kullanıcı kararı (0.18): haritadan uçak konumlandırma istenmedi |
| Uçuş planı / IOS özellikleri ayrı iş değil; ileride GNS 430 eklentisine **harita API'si** yazılacak | Kullanıcı kararı (0.18): genel harita eklentisi, API ayrı görev olarak verilecek |

## 2. Kullanıcıyla çalışma kuralları

- Kullanıcıyla **Türkçe** konuş. Kullanıcı X-Plane eklenti geliştirmede yeni;
  kodu **anlayarak** ilerlemek istiyor: her adımın sonunda ne yaptığını ve
  **neden** yaptığını sade anlat.
- **Kararlar birlikte alınır.** Proje klasöründe yazma/silme yetkin tam, ama
  kullanıcı her kritik kararı, nedenini, çekinceleri ve planı bilmeli. Koda
  başlamadan önce planı (seçenekler, artı/eksi, önerin) sun; gerçek tasarım
  seçimlerinde onay bekle. Sessiz mimari karar verme.
- Önemli kararlarda ve yeni araç/kütüphane kurmadan önce sor. Kullanıcının
  "sen nasıl rahatsan" dediği öneriler de yapılması beklenen işlerdir.
- Kod **insan okuyucu için** yazılır: açık adlar, ne/neden anlatan Türkçe
  yorumlar, sıkıştırılmış "zekice" kod yok.
- Her kilometre taşında `docs/MIMARI.html` + `MIMARI.pdf`'e yeni kritik
  bölümleri açıklamalarıyla ekle.
- **X-Plane'de denenmemiş hiçbir şeyi "çalışıyor" diye raporlama.** "Derlendi",
  "testler geçti", "önizlemede görüldü" ve "X-Plane'de denendi" ayrı şeylerdir.
  X-Plane'de denenmesi gereken her işin sonunda kısa bir test listesi ver,
  sonucu bekle. Log.txt'yi kendin okuyabiliyorsan oku.
- **Biçim tahmin etme.** apt.dat / DSF / earth_awy.dat için önce gerçek dosyaya
  bak (`tools/dsfinfo`, dosyanın kendi satırları). Gerçek veri spesifikasyondan
  farklı olabiliyor (bölüm 7).
- Her iş bitince: derle (`/W4` uyarısız), testleri çalıştır, bu dosyayı ve
  gerekiyorsa `README.md` / `docs/MIMARI.html` + `MIMARI.pdf`'i güncelle,
  **commit + push** (`origin main`, github.com/efeegrkm/X-Plane-Map-Plugin).
  Commit mesajları **İngilizce**.

## 3. Kod standardı (kesin, kullanıcının isteği)

- **Önekler:** `m` üye değişken (struct alanları dahil: `LatLon::mLat`), `p`
  parametre, `t` yerel/geçici (döngü sayaçları dahil: `tIndex`), `k` sabit,
  `g` global.
- **Süslü parantez her zaman alt satırda**: sınıf, fonksiyon, namespace,
  if/else, döngü, switch, **lambda**. Tek satırlık `if` bile parantezli.
- Tür ve fonksiyon adları **PascalCase** (`MapView::ZoomAt`). Satır en çok
  **120 karakter**.
- **İngilizce:** arayüz metni, log, menü, komut, ayar anahtarı, dosya adı,
  katman kimliği, tanımlayıcılar. **Kod yorumları Türkçe ve ASCII** (ş→s,
  ğ→g, ı→i, ç→c, ö→o, ü→u). `README.md`, `AGENTS.md`, `docs/MIMARI.html`
  Türkçe (normal Türkçe harflerle).
- C++20 (`std::format`, `std::to_array`), `/W4` uyarısız, statik CRT.
- XPLM callback'lerine refcon olarak `this` verilir; global durum yok.
- Dış kod sadece `third_party/` altına; kendi kodla karıştırılmaz.
- Log: `Log("x = {:.1f}", tX)` (std::format). Sadece ana thread.

```cpp
void Example(const std::vector<int>& pValues)
{
    for (size_t tIndex = 0; tIndex < pValues.size(); ++tIndex)
    {
        if (pValues[tIndex] == kLimit)
        {
            break;
        }
    }
    auto tLess = [](int pA, int pB)
    {
        return pA < pB;
    };
}
```

## 4. Ortam

| | |
|---|---|
| Simülatör | X-Plane 11.55r2 demo, `C:\X-Plane 11` |
| Kurulu sahne | Sadece `Global Scenery/X-Plane 11 Demo Areas` (35 DSF: Hawaii, Seattle +46..+48 / -121..-124, Juneau) + Global Airports. **Türkiye yok.** DSF'ler 7z sıkıştırılmış |
| Proje | `C:\Projects\QT\prime_map_plugin` |
| Derleyici | MSVC 2022 x64; CMake + Ninja Qt Creator'ın araçlarından (kit "Desktop Qt 6.10.1 MSVC2022 64bit"). **Qt kütüphanesi kullanılmıyor**, Qt Creator sadece IDE |
| SDK | `third_party/XPSDK` (sadece XPLM200, 210, 300, 301 tanımlı; XP12 API'si kullanılırsa derleme hata verir — istenen davranış) |
| Ekran kartı | RTX 5070 Laptop, Log: "OpenGL ready: 4.6.0 NVIDIA" |
| Log | `C:\X-Plane 11\Log.txt`, bizim satırlar `[prime-map-plugin]` ile başlar |

## 5. Derleme, test, kurulum

Kullanıcının `build/` klasörü Qt Creator'ındır; **ona dokunma**, kendi
derlemeni geçici bir klasörde yap:

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
C:\Qt\Tools\CMake_64\bin\cmake.exe -S C:\Projects\QT\prime_map_plugin -B <derleme_klasoru> -G Ninja ^
  -DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe -DCMAKE_BUILD_TYPE=Release ^
  -DMAP_TESTS=ON -DMAP_PREVIEW=ON -DMAP_DEPLOY=ON
C:\Qt\Tools\Ninja\ninja.exe -C <derleme_klasoru>
<derleme_klasoru>\map_tests.exe
```

| CMake seçeneği | Ne |
|---|---|
| `MAP_DEPLOY` (ON) | Derlemeden sonra `win.xpl`'yi `C:\X-Plane 11\Resources\plugins\prime-map-plugin\64\`'e kopyalar. X-Plane açıkken dosya kilitlidir: önce `tasklist \| findstr X-Plane` |
| `MAP_TESTS` | `map_tests.exe`: X-Plane'siz testler. Çıktı `N checks passed, 0 failed` olmalı (2026-11-07: 30.513 kontrol) |
| `MAP_PREVIEW` | `map_preview.exe` ve `dsf_info.exe` |
| `XPLANE_DIR`, `XPSDK` | Farklı X-Plane / SDK klasörü |

- **Önizleme** (X-Plane'siz görsel kontrol): `map_preview --lat 47.45 --lon
  -122.30 --zoom 12 --mode vfr --out ksea.ppm` (ayrıca `--size --show <katman>
  --hide <katman> --panel <grup> --describe x,y --demo-drawings --no-ui
  --cache --xplane --terrain-colors <ini>`). Önizlemenin kendi açılışı (nav
  verisini dosyadan okuma) 10–35 s sürer; zemin süreleri için log'daki
  `terrain ...` satırlarına bak, toplam süreye değil. PNG'ye: `magick ksea.ppm ksea.png`. Gerçek X-Plane
  denemesinin yerine geçmez.
- **DSF inceleme:** `dsf_info <dosya.dsf> [--raw out.dsf] [--dump-terrain <indeks> out.txt]`
  (özellikler, dışlamalar, arazi/nesne/poligon/ağ envanteri, aşama süreleri).
- **Mimari PDF'ini yeniden üretme** (proje kökünden):
  `"C:\Program Files\Google\Chrome\Application\chrome.exe" --headless=new
  --disable-gpu --no-pdf-header-footer --print-to-pdf="MIMARI.pdf"
  "docs\MIMARI.html"`
- X-Plane'de yeni sürümü denemek: X-Plane kapalıyken kur, ya da Developer →
  Reload plugins.
- Shell notu: Bash heredoc'u kaynak dizgilerindeki `\n`'i bozabiliyor; dosyaya
  C++ dizgisi yazarken düzenleme aracını veya bir Python betiğini (`python -I`)
  kullan.

## 6. Yasaklar

- Depoda dokunma: `build/` (kullanıcının derlemesi), `.qtcreator/`.
- X-Plane klasöründe **sadece** `Resources\plugins\prime-map-plugin\` altına yaz.
- `Custom Scenery\scenery_packs.ini`'ye **asla** yazma (sadece okunur; X-Plane
  her açılışta kendisi yeniden yazar).
- Dış harita/karo servisi, internetten harita verisi, `XPLMMap` API'si,
  XP12'ye özel SDK fonksiyonu kullanma.
- Arka plan thread'inde XPLM fonksiyonu veya `Log` çağırma.
- `src/gfx/` dışında OpenGL çağırma.

## 7. Doğrulanmış biçim ve SDK gerçekleri

Bunlar gerçek dosyalarda / X-Plane'de görüldü; kodda `GERCEK VERI` notlarıyla
işaretli. Değiştirmeden önce oku.

**SDK**
- ILS/LOC yönü **gerçek** yön (manyetik değil); TACAN frekansı kanal numarası
  (NUW = 85); "----" kimlikli yer tutucu ILS kayıtları var (filtreleniyor).
- VOR kaydının "heading" alanı `NavRecord::mVariation` olarak kullanılıyor
  (slaved variation, + = doğu varsayımı). **Doğrulanmadı**: Log'daki
  `VOR sample:` satırlarına bak (Seattle civarı ~+19 beklenir).

**apt.dat (Global Airports: 325 MB, ~29.900 havalimanı, 7,25 M düğüm)**
- Düğüm satırı sonunda 0–3 stil değeri olabilir ("51 59 105" = 2 boya + ışık;
  1–99 boya, 101–108 ışık).
- 21 satırı 6 alan; 1202'de "taxiway_A".."F"; 1300 tipi "tie_down"; 14
  yüksekliği ondalık; başlıktaki kule alanı hep 0 (kule = 54/1054 satırı);
  dosya ortasında "1130 Generated by..." satırı olabilir.
- **Bezier kuralı** (spesifikasyonda yok, ölçüldü): kontrol noktası düğümden
  ÇIKAN eğriye ait, gelen eğri için aynası (2P − C).
- Heliport'ların çoğunda yüzey düğümü yok, sadece 102 (helipad) satırı var.
- 156.345 yüzeyden 1'i üçgenlenemiyor (bilinen).

**DSF (XP11 demo dosyaları)**
- POOL/SCAL değeri = ham / 65535 × ölçek + kaydırma; nested polygon range
  komutu sayaç+1 indeks okur; DEM "elevation" 1201², int16, satır 0 = GÜNEY,
  sütun 0 = batı.
- Karo önceliği: Custom Scenery (`scenery_packs.ini` sırası), sonra Global;
  overlay olmayan ilk DSF karonun zeminidir.

**DSF overlay içeriği (Aşama 0 envanteri, 2026-11-06, `dsf_info`)**
- Demo temel DSF'lerinde (+47-122, +47-123, +58-135) nesne (OBJT) yok; POLY:
  `lib/g8/*.for` orman (+47-123: ~30 bin poligon, ~470 bin köşe),
  `lib/g8/beaches.bch` kıyı şeritleri, `lib/g10/autogen/*.ags` (urban_*,
  ind_*, park_*, natural), az sayıda `*.fac`. Tek NETW: `lib/g10/roads.net`.
- Yol alt türleri (`roads.net` içindeki `#VROAD` yorumlarından): 100–131
  otoyol, 10–28 primary, 30–48 secondary, 50–71 local, 151–154 demiryolu,
  220 enerji hattı.
- Ağ komutları 32 bitlik havuz kullanır (`PO32`/`SC32`; belgeye göre 4 düzlem:
  lon, lat, yükseklik, kavşak kimliği, 0 = şekil noktası). Okuyucu bunları
  henüz okumuyor.
- Global Airports overlay'leri: nesne + dışlama (`sim/exclude_obj/fac/for/net/
  pol/lin/str/bch`; +47-123'te for 148, net 188); üçgen ve DEM yok.
- Nested polygon range (15): belge "N indeks" der, gerçek dosyada sayaç + 1
  indeks (tüm akış böyle sonuna kadar hatasız okunuyor).

**X-Plane'in kendi harita verisi**
- `Resources/map data/` altında sadece `water/<10°>/<karo>.shp+.shx`: standart
  ESRI Polygon (tip 5), karo başına. Demo'da sadece kurulu 35 karo. DSF kıyısının
  sadeleştirilmiş hâli (shp→DSF medyan 10–15 m, %99 < 90 m) ve sadece büyük su
  kütleleri (dağ karosu +47-122: DSF 2.450 km kıyı, shp 579 km). Su için esas
  DSF'tir (kullanıcı kararı).
- `Resources/dsf_files`: 5 bayt başlık (anlamı bilinmiyor) + 360×180 bayt,
  boylam öncelikli: indeks `(lon + 180) * 180 + (lat + 90)`; 255 = global
  sahnede DSF'i olan karo, 0 = yok (okyanus). Kurulu olup olmadığını söylemez.
- `.ter`: `BASE_TEX` (.dds, DXT5, mipmap'li), `PROJECTED <m> <m>`, `BORDER_TEX`.
  `library.txt`'te `REGION_DEFINE/REGION_BITMAP/REGION` blokları: aynı sanal ad
  bölgeye göre farklı `.ter`'e gidebilir.

**Rakım ve sea_level (Aşama 1, 2026-11-07)**
- DSF raster katmanları (DEMN): `elevation` 1201², int16, post-centric,
  ölçek 1; `sea_level` 256², int16, hücre merkezli, satır 0 = güney;
  `bathymetry` adı var ama DEMI/DEMD atomu yok (boş).
- **Komşu karoların ortak kenar noktaları aynı değil** (ortalama 1–2 m, en çok
  ~30 m). Örnekleyici ortak kenarda var olan karoların ortalamasını alır.
- `sea_level`: denizde deniz tabanı yüksekliği gibi (Puget Sound en derin
  −242 m); verisi olmayan deniz −10 (Juneau kanallarının çoğu), göllerde ≥ 0,
  karada anlamsız (kullanılmaz). Biçim belgelenmemiş; derinlik tonu sadece
  −10'dan derinde.
- LZMA (satır farkı + bayt düzlemi, gerçek DEM): seviye 0–4 hızlı mod,
  ~0,75 MB, 240–310 ms; seviye 5 normal mod 0,68 MB ama 5 kat yavaş. Seçim 4.
  Açma ~50 ms.

**DSF vektörleri (Aşama 2, 2026-11-07)**
- Dışlama özelliği değeri: `"batı/güney/doğu/kuzey"` (derece), ör.
  `sim/exclude_for = -122.477855/47.454691/-122.476225/47.461676`.
- Yol zincirleri `PO32` havuzundan; ölçek 16 bitlikle aynı kural varsayıldı
  (ham / (2³²−1) × ölçek + kaydırma). Önizlemede yollar kıyı ve havalimanlarıyla
  hizalı çıktı: varsayım gerçek veriyle tutuyor. Komut 9/10 indekslere junction
  offset ekler, 11 eklemez (belge).
- KSEA karosunda (+47-123) kullanılan: 56.325 alan (orman + yerleşim + park),
  274.078 yol/demiryolu/enerji hattı zinciri.
- **Kaspersky**, taze derlenmiş `dsf_info.exe`'yi `VHO:Trojan.Win64.Loader.gen`
  diye yakaladı (sezgisel yanlış alarm: imzasız, 7z açıcı içeren yeni exe).
  Kullanıcıya derleme klasörlerini istisna yapması önerildi; exe karantinaya
  alınırsa sadece yeniden derle.

**Süreler (önbelleksiz karo):** 7z açma 150–490 ms, DSF Open 8–16 ms, üçgen
geçişi 17–21 ms, DEM 2 ms; karo üretimi toplam 0,4–0,5 s; yakın zoom üçgen
deposu 0,2–0,6 s. 0.16 ek maliyet: rakım sıkıştırma ~0,3–0,4 s/karo (bir kez).
Görüntü: seviye 3 parçası (1024²) ~0,34 s, seviye 2 karo ~0,39 s, seviye 0
1 ms; önbellekten raster + rakım ~60–75 ms (2026-11-07 ölçümü makinede başka
yoğun bir uygulama açıkken yapıldı; gerçek değerler daha düşük olabilir).

**earth_awy.dat (1100)**: satır = segment; uç koordinatı yok, kimlik + bölge +
tür ile nav verisinden çözülür (104.581 segmentin hepsi çözülüyor). Önce
`Custom Data`, sonra `Resources/default data`.

## 8. X-Plane ile yaşamak: bilinen tuzaklar

1. `XPLMDrawString` doku açar ve X-Plane GL_CULL_FACE'i açık bırakabilir:
   her geometri çiziminden önce `XPLMSetGraphicsState` + cull kapalı.
   `gfx/Canvas` bunu kendisi yapar; Canvas dışından çizim yapma.
2. Canvas her çizimde X-Plane'in program/VAO/VBO/cull durumunu kaydedip geri
   koyar; bunu bozma.
3. **Pop-out penceresi ayrı OpenGL bağlamıdır.** Buffer/doku/shader paylaşılır,
   **VAO paylaşılmaz**; başka bağlamın VAO'sunu bağlamak X-Plane'i çökertir.
   VAO'lar `gfx/ContextVertexArray` ile bağlam başına tutulur. Bağlam
   paylaşımsızsa Canvas çizmez, bir kez loglar.
4. Boxel ≠ piksel (UI ölçeği): scissor için `gfx/GlScreen`.
5. Doku numarası `XPLMGenerateTextureNumbers`, bağlama `XPLMBindTexture2d`
   (doğrudan `glBindTexture` değil).
6. Yazılar sadece ASCII (Türkçe harf, derece işareti çizilmez). SDK
   fonksiyonlarına değiştirilebilir `char` dizisi ver.
7. Tıklamayı alttaki pencereye geçirme (fare takibi bozulur); tekerlek için
   0 döndürmek sorun değil. `XPLMCreateWindowEx`'e bütün callback'ler verilmeli.
8. apt.dat sayıları yerel ayardan bağımsız okunmalı (`AptLine`); Türkçe
   Windows'ta `strtod` virgül bekleyebilir.
9. Zemin önbellek biçimi değişirse `TerrainRaster.cpp` `kFileVersion`'ı
   (şu an 3) veya `TerrainElevation.cpp` `kFileVersion`'ı (şu an 1),
   varsayılan zemin renkleri değişirse `TerrainPalette::kPaletteVersion`'ı
   artır; eski dosyalar kendiliğinden yenilenir.
10. `third_party/earcut`'ta tek bir belgelenmiş hız yaması var
    (`isValidDiagonal` test sırası; sonuç aynı). earcut'ı güncellersen yamayı
    yeniden uygula (`third_party/earcut/README.txt`).
11. `FindVisibleAirports(..., pNeedNodes)`: nesne katmanı `false` verir, çünkü
    sadece helipad'li heliport'ların düğümü yoktur.
12. Rakım belleği iki katmanlı (`TerrainStore`): tam ızgara 12 karo, kaba
    (seviye 2+) 64 karo; `ElevationFor(..., pLevel)` seviyeye göre arar.
    `ElevationGrid::DropFineLevels` sonrası ince seviye okunamaz, `Save` yapılamaz.
    Zemin dokusu komşu karonun rakımı olmadan üretilmişse
    (`TerrainImage::mMissingNeighbors`), komşu hazır olunca katman onu yeniden
    ister. Bir kez hazır olup artık bulunamayan komşu eksik sayılmaz; bu kuralı
    bozarsan sonsuz yeniden üretim olur (`TerrainStore::BuildImageFor`).
13. Bash heredoc (`<<'EOF'`) içindeki Python betiklerinde `\\n` gerçek satır
    sonuna dönüşebiliyor (iki kez görüldü). Betiği önce Write ile dosyaya yaz,
    `python -I betik.py` ile çalıştır.

## 9. Mimari özeti

Ayrıntı ve şemalar `MIMARI.pdf`'te. Kısaca:

```
X-Plane ─► Plugin.cpp (giris, komut, flight loop: depolari ilerletir)
              │                         │
              ▼                         ▼
     src/data depolari          MapWindow (pencere, fare, ui/)
     (arka plan thread'leri)        │ DrawContext {MapView, Canvas, LabelPlacer, MapTheme}
              │ const &             ▼
              └──────────────► LayerManager ─► layers/* ─► gfx/Canvas (GL 3.3)
```

- **Çekirdek X-Plane'e bağımlı değil**; XPLM sadece `Plugin.cpp`,
  `MapWindow.*`, `data/NavLoader`, `data/AircraftData`, `layers/FmsLayer`,
  `Log.cpp`, `gfx/Canvas.cpp`, `gfx/Texture.cpp`, `ui/UiDraw.cpp`. Bu sayede
  çekirdek testlerde ve önizlemede çalışır. CMake: `CORE_SOURCES` /
  `PLUGIN_SOURCES`.
- **İstekle veri:** katmanlar `AirportDetailStore::Use(i)` /
  `TerrainStore::Request` ile ister; hazır değilse yer tutucu çizer; flight
  loop istekleri işçiye verir. Çizim hiç beklemez.
- **Katman sırası:** `Plugin.cpp::CreateLayers()`. Etiket önceliği
  `LabelPriority` (havalimanı 10, pist 15, VOR 20, NDB 30, ILS 40, fix 60,
  hava yolu 70); `LabelPlacer` çakışanı reddeder.
- **Alt katmanlar:** `Layer::AddSubLayer` / `IsSubLayerVisible`; ayar anahtarı
  `<katman>.<alt>`. Kullananlar: AirportSurface, AirportObjects, Runway,
  Drawing.
- **Panel kuralları** (`MapWindow::ApplyLayerPanelHit`): kapalı bir katmanın
  alt katmanı açılınca sadece o alt katman açılır (katman "yarım" görünür);
  grup kutusu sadece bu modda varsayılan açık katmanları açar (Taxi routes gibi
  varsayılan kapalılar tek tek açılır), hepsi açıksa gruptaki her şeyi kapatır.
  Varsayılan kapalı katmanlar eski `[layers]` bölümünden okunmaz.
- **Tema:** `MapTheme` (mod başına renk, zemin biçimi, varsayılan katmanlar,
  0.16'dan beri "veri yok" rengi `mNoData`); katmanlar renkleri
  `pContext.mTheme`'den okur. Varsayılan katmanlar (0.18) `MapTheme.cpp`
  başındaki tablolarda (`std::span`): kullanıcının kaydettiği ayarlar; yeni katman / alt
  katman eklenirse beş tabloya da satır ekle (yoksa `Layer`'ın kendi varsayılanı).
- **Zemin (0.16):** `TerrainRaster` sadece sınıf raster'ı; rakım
  `TerrainElevation` (1201² + piramit, `*.elevation`, `ElevationCodec`).
  Görüntü üreticileri ortak ara sonuç doldurur, son adım `ComposeImage`
  (kabartma, VFR bandı, su tonu, kıyı çizgisi; formüller `TerrainRelief`).
  Deniz / veri yok: `WorldTileMap` (`dsf_files`) → `SceneryIndex::NoBaseCoverage`.
  Ayrıntı MIMARI bölüm 9.5–9.7.
- **Yeni katman:** `Layer`'dan türet → `CreateLayers()`'a ekle → CMake →
  `MapTheme.cpp` varsayılan görünürlük → `ui/LayerPanel.cpp` `kGroups`.

## 10. Ayar dosyaları (eklenti klasörü)

`C:\X-Plane 11\Resources\plugins\prime-map-plugin\`: `map_settings.ini` (`[window]`,
`[view]` + `mode`, `[tools]`, `[layers.scenery|vfr|ifr_low|ifr_high|dark]`; eski
`[layers]` Scenery'ye okunur; `[cache] terrain_mb` zemin önbelleği sınırı, varsayılan 1024), `map_drawings.txt`, `terrain_colors.ini`
(`[colors] [shading] [blend] [warp] [closeup] [relief] [water] [vfr]
[palette]`), `cache/terrain/` (`*.terrain`, `*.elevation`, `*.vectors`, `*.nobase`; `CacheTrim` en eski karoları siler),
`cache/airports/airport_index.bin` (önbellekler silinebilir).

## 11. Sürüm geçmişi ve durum

| Sürüm | İçerik | X-Plane'de |
|---|---|---|
| 0.4–0.6 | Pencere, Web Mercator, kaydırma/zoom, ızgara, uçak, katman paneli; navaid'ler, etiket seyreltme; pistler | denendi |
| 0.7 | İngilizce arayüz, MapPlugin adı; ILS tüyü eşikten; çizimler, iz, FMS, hava yolları | kısmen (çizimler, hava yolları, ILS görüldü) |
| 0.8 | Zemin: kurulu DSF'ten arazi + kabartma | denendi (Seattle) |
| 0.9 | OpenGL 3.3, C tarzı kod temizliği | denendi |
| 0.10 | Kendi apt.dat okuyucusu, havalimanı yüzeyleri | denendi ("gayet güzel") |
| 0.11–0.12 | Havalimanı nesneleri, tooltip, indeks önbelleği; ışıklar, taksi ağı, gruplu panel | denendi ("güzel") |
| 0.13 | Harita modları, VOR pusula gülü, helipad'li heliport'lar | kullanıcı "güzel gözüküyor" dedi; ayrıntılı test listesi raporlanmadı |
| 0.14–0.15 | (kullanıcının commit'i `4ee9d95`) Yakın zoom zemini (seviye 4–5, DSF üçgenlerinden), bikubik gölge, mipmap, sınır kıvırma, kara renk karışımı; alt katmanlar; yüzen çizim paleti; pop-out VAO düzeltmesi; zoom eşikleri bir kademe erken, en çok zoom 16.5 | denendi (kullanıcı: zemin kalitesiz → zemin görevi) |
| 0.16 | Zemin Aşama 0 (DSF overlay envanteri, `dsf_info`) + Aşama 1: tam çözünürlüklü DEM + piramit, piksel başına çok yönlü kabartma, komşu karolu örnekleme ve yenileme, VFR sectional bantları, kıyı çizgisi / şeridi / derinlik tonu, `dsf_files` ile deniz / veri yok, `[relief] [water] [vfr]` ayarları | denendi: sorunsuz; kullanıcı "gelişme var ama yeterince iyi değil" |
| 0.17 | Aşama 3: sınıf renkleri X-Plane'in `.ter` dokularından (`TerrainPaletteSource`, `[palette] source`). Aşama 2: DSF vektörleri (`TerrainVectors`, `.vectors` önbelleği, dışlamalar): orman ve yerleşim zemine işlenir (Terrain alt katmanları Forests / Built-up areas; VFR'da yerleşim sarı), yollar / demiryolu / enerji hattı yeni `RoadLayer` (Roads and railways, 6 alt katman). Raster önbelleği v4, palet v3 | denendi: yerleşim simülatör için önemsiz bulundu (varsayılan kapalı kalsın) |
| 0.18 | Kullanıcının kaydettiği katman ayarları varsayılan oldu (`MapTheme.cpp` tabloları); yeni **Dark** modu (siyah kara, lacivert su, minimal); **Go to airport** paneli (`AirportSearchPanel`, `AirportSearch`; sadece harita gider, klavye odağı panel açıkken); alt şeritte `Loading terrain: N images` (kaba / boş zemin görünen her an); zemin önbelleğine disk sınırı (`CacheTrim`, `[cache] terrain_mb` = 1024); ad **prime-map-plugin** (klasör `plugins/prime-map-plugin`, log `[prime-map-plugin]`, menü "Prime Map Plugin", imza `ozdev.prime_map_plugin`; komutlar `ozdev/map/...` aynı kaldı) | **denenmedi** |

Hiç yapılmamış: FPS ölçümü (Diagnostics modu var), Vulkan modunda deneme.

### Açık konular
- VOR sapması işaret/kaynak doğrulaması (bölüm 7).
- Mod renkleri FAA geleneğinden; X-Plane'in kendi haritasıyla karşılaştırılıp
  ayarlanmadı (kullanıcıdan ekran görüntüsü istenebilir).
- IFR Low'da zoom 9'da fix'ler yoğun. MEF rakamları, hava sahası sınırları yok.
- Zemin önbelleği sınırı 0.18'de eklendi (`CacheTrim`, 1 GB). Önbellekteki karo okununca
  `.terrain` dosyasının değiştirilme zamanı güncellenir (LRU sırası buna dayanır).
  Eski `HaritaPlugin` klasörü artık yok (2026-11-07 kontrol edildi).

### Zemin yol haritası (kullanıcı onayladı: 0 → 1 → 3 → 2, sonra 4)
- **Aşama 0** (bitti) envanter; **Aşama 1** (bitti, X-Plane'de denenmedi)
  kabartma ve su. Kararlar: su için DSF esas (`map data/water` sadeleştirilmiş,
  sadece kurulu olmayan karo / hızlı ilk görüntü için); kıyıya sahte ayrıntı
  yok; su tonu = kıyı şeridi + `sea_level` derinliği; VFR bantları sectional.
- **Aşama 3** (bitti, 0.17): sınıf başına ortalama renk, global `terrain10`
  setinden (bölgesel `REGION` kopyaları kullanılmadı; kullanıcı "sınıf başına
  ortalama" seçti). Sonuç `cache/terrain/xplane_palette.txt`.
- **Aşama 2** (bitti, 0.17): kararlar: orman zemine işlenir; varsayılan
  Scenery hepsi açık, VFR yol + demiryolu + yerleşim, IFR kapalı; enerji hattı
  her modda kapalı. Kalan: `beaches.bch` kullanılmadı; VFR yerleşim sarısı
  autogen blokları yüzünden lekeli (birleştirme / yumuşatma düşünülebilir);
  `DsfReader`'da üç komut döngüsü (`ForEachTriangle`, `CountCommands`,
  `ForEachVector`) hâlâ ayrı (teknik borç).
- **Aşama 4:** iki işçi, merkezden kenara öncelik, nesil sayacıyla iptal, rota
  önyükleme, önceden önbellek komutu; hedefler Aşama 0 ölçümüne göre (önbellekte
  < 0,1 s, kaba ilk görüntü < 0,1 s hızlı yoldan, karo başına tam görüntü
  ≤ 0,6 s).

### Sıradaki işler
Kullanıcı (0.18): "IOS işi" diye ayrı bir başlık açılmayacak; genel harita
eklentisi. İleride GNS 430 eklentisi için harita API'si yazılacak (ayrı görev).
Uçak konumunu değiştiren özellik istenmiyor. Aşağıdaki liste eski öneri listesidir,
kullanıcı seçmeden başlama:
Genel bir X-Plane eklentisi yazıyoruz; X-Plane'in içinde, onunla aynı
bilgisayarda çalışır. IOS ekranının aynı/ayrı bilgisayarda olması bizim
konumuz değil, **sorma**; ağ üzerinden ayrı uygulama planlanmıyor.
1. Uçak bilgi kutusu (irtifa, hız, yön, dikey hız, transponder) + 1/2/5 dk
   tahmin çizgisi + mesafe halkaları
2. Ara / git (ICAO veya navaid kimliği)
3. Cetvel (mesafe / yön ölçme)
4. Haritadan uçak konumlandırma (pist başı, kalkış noktası, serbest nokta)
5. Uçuş kaydı + debrief (geri oynatma, olay işaretleri, CSV/KML)
6. AI / multiplayer trafik
7. Hava durumu katmanı
8. SID / STAR / yaklaşma usulleri (CIFP)
9. Okunurluk: yazı ölçeği, büyük düğmeler, gece teması (X-Plane pencereleri
   dokunmatik hareket tanımıyor; dokunuş fare tıklaması gibi gelir)
