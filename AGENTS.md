# AGENTS.md — Bu projede çalışacak yapay zekâ için devir notu

Bu dosya, projeye sıfırdan giren bir yapay zekâ asistanının (Claude, Codex,
Cursor, Copilot ...) ihtiyaç duyduğu her şeyi tek yerde toplar. Önce bunu,
sonra `MIMARI.md`'yi (kod haritası), gerektiğinde `README.md` (kullanım, test
listesi) ve `CLAUDE.md`'yi (ayrıntılı karar geçmişi) oku.

Durum tarihi: 2026-11-05, sürüm **0.13**, son commit `63ed9f3`.

## 1. Proje nedir?

X-Plane 11 için bir harita eklentisi (`win.xpl`, C++20). Dış harita servisi
(OSM vb.) **kullanmaz**; sadece X-Plane'in kendi verisini (apt.dat, DSF sahne,
nav veritabanı, earth_awy.dat, dataref'ler) okur ve OpenGL 3.3 ile **kendi
penceresinde** çizer. Katmanlar gruplu ve tek tek açılıp kapanır; dört harita
modu var (Scenery, VFR, IFR Low, IFR High).

**Asıl kullanım:** kullanıcının simülatörlerinin **eğitmen istasyonu (IOS)
ekranları**. Yeni özellik önerirken eğitmen iş akışını düşün (konumlandırma,
trafik, debrief, büyük ekranda okunurluk).

## 2. Kullanıcıyla çalışma kuralları

- Kullanıcıyla **Türkçe** konuş. X-Plane eklenti geliştirmede yeni; kodu
  anlayarak ilerlemek istiyor: her adımın sonunda ne yaptığını ve **neden**
  yaptığını sade anlat.
- Önemli kararlarda sor. Kurulum (yeni araç/kütüphane) yapmadan önce sor.
- **X-Plane'de denenmemiş hiçbir şeyi "çalışıyor" diye raporlama.** Derlendi,
  testler geçti, önizlemede görüldü ayrı şeyler; "X-Plane'de denenecek" diye
  test listesi ver ve kullanıcının sonucunu bekle.
- "Sen nasıl rahatsan" denilen öneriler de yapılması beklenen işlerdir.
- Biçim tahmin etme: apt.dat / DSF / earth_awy.dat için önce gerçek dosyaya
  bak. Gerçek veri spesifikasyondan farklı olabiliyor (`CLAUDE.md`
  "Doğrulanmış biçim" bölümü).
- İş bitince: derle, testleri çalıştır, uyarıları temizle, `CLAUDE.md` ve
  gerekiyorsa `README.md`'yi güncelle, **commit + push** (`origin main`,
  github.com/efeegrkm/X-Plane-Map-Plugin). Commit mesajları İngilizce.

## 3. Kod standardı (kesin)

- Önekler: `m` üye (struct alanları dahil), `p` parametre, `t` yerel
  (döngü sayaçları dahil: `tIndex`), `k` sabit, `g` global.
- Süslü parantez **her zaman alt satırda**: sınıf, fonksiyon, namespace,
  if/else, döngü, switch, **lambda**. Tek satırlık `if` bile parantezli.
- Tür ve fonksiyonlar PascalCase. Satır en çok **120 karakter**.
- Arayüz metni, log, komut, ayar anahtarı, dosya adı, katman kimliği
  **İngilizce**. **Kod yorumları Türkçe ve ASCII** (ş→s, ğ→g, ı→i, ç→c,
  ö→o, ü→u). README.md, CLAUDE.md, MIMARI.md, bu dosya Türkçe.
- `/W4` uyarısız. XPLM callback'lerine refcon olarak `this`; global durum yok.
- Dış kod sadece `third_party/` altına (kendi kodla karıştırılmaz).
- OpenGL çağrısı sadece `src/gfx/` içinde. Yazılar `XPLMDrawString`: sadece
  ASCII (Türkçe harf, derece işareti çizilmez).
- Arka plan thread'inde XPLM fonksiyonu ve `Log` **çağrılmaz**; iş rapor
  metni döndürür, ana thread loglar.

```cpp
void Example(int pCount)
{
    for (int tIndex = 0; tIndex < pCount; ++tIndex)
    {
        if (tIndex == kLimit)
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

## 4. Dokunma / yazma yasakları

- Depoda dokunma: `_qt_template_yedek/`, `_to_delete/`, `build/` (kullanıcının
  Qt Creator derlemesi), `.qtcreator/`. `legacy/` sadece başvuru.
- X-Plane klasöründe (`C:\X-Plane 11`) **sadece**
  `Resources\plugins\MapPlugin\` altına yaz.
- `Custom Scenery\scenery_packs.ini`'ye **asla** yazma (sadece okunur).

## 5. Ortam ve derleme

- Windows 11, X-Plane 11.55r2 demo, `C:\X-Plane 11`. Kurulu sahne sadece demo
  bölgeleri (Seattle, Hawaii, Juneau) + Global Airports. Türkiye sahnesi yok.
- MSVC 2022 + CMake + Ninja (Qt Creator'ın araçları; Qt kütüphanesi
  **kullanılmıyor**). SDK `third_party/XPSDK` (XPLM200..301; XP12 API'si yok).
- Komut satırından derleme (kendi derleme klasörünü depo dışında/geçici bir
  yerde aç, `build/`'i kullanma):

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
C:\Qt\Tools\CMake_64\bin\cmake.exe -S C:\Projects\QT\prime_map_plugin -B <derleme_klasoru> -G Ninja ^
  -DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe -DCMAKE_BUILD_TYPE=Release ^
  -DMAP_TESTS=ON -DMAP_PREVIEW=ON -DMAP_DEPLOY=ON
C:\Qt\Tools\Ninja\ninja.exe -C <derleme_klasoru>
<derleme_klasoru>\map_tests.exe
```

- `MAP_DEPLOY=ON` derlemeden sonra `win.xpl`'yi
  `C:\X-Plane 11\Resources\plugins\MapPlugin\64\`'e kopyalar. X-Plane açıksa
  dosya kilitlidir: önce `tasklist | findstr X-Plane` ile kontrol et.
- Testler: `map_tests` → **355 kontrol**, hepsi geçmeli.
- Görsel kontrol (X-Plane'siz): `map_preview --lat 47.45 --lon -122.30
  --zoom 12 --mode vfr --out ksea.ppm` (ayrıca `--show`, `--hide`, `--panel`,
  `--describe x,y`). Önizleme gerçek X-Plane'in yerine geçmez.
- Shell notu: Bash heredoc'u kaynak dizgilerindeki `\n`'i bozabiliyor; dosyaya
  dizgi yazarken düzenleme aracını veya bir Python betiğini kullan.

## 6. Mimari özeti

Ayrıntı `MIMARI.md`'de. Kısaca:
- `Plugin.cpp` (giriş, komutlar, flight loop) → `MapWindow` (pencere, fare,
  UI) → `LayerManager` → `layers/*` → `gfx/Canvas`.
- Veri `src/data/` içinde, arka plan thread'leriyle hazırlanır: kendi apt.dat
  okuyucumuz (indeks + istek üzerine ayrıntı), DSF zemin okuyucu (7z + DSF →
  raster + disk önbelleği), SDK'dan nav verisi, earth_awy.dat.
- Harita modları `src/MapTheme.*`: her katman renklerini
  `pContext.mTheme`'den okur. Mod başına katman görünürlüğü
  `map_settings.ini` `[layers.<mod>]`.

## 7. Şu anki durum

| Sürüm | İçerik | X-Plane'de |
|---|---|---|
| 0.4–0.10 | Pencere, katmanlar, navaid, hava yolları, çizimler, FMS, iz, zemin, GL 3.3, apt.dat okuyucu | denendi, kullanıcı memnun |
| 0.11–0.12 | Havalimanı nesneleri, tooltip, ışıklar, taksi ağı, gruplu panel | denendi, "güzel"; helipad isteği düzeltildi |
| 0.13 | Harita modları, VOR pusula gülü, sadece helipad'li heliport'lar | kullanıcı "güzel gözüküyor" dedi; ayrıntılı test listesi (README) raporlanmadı |

**Açık konular / doğrulanmamış varsayımlar**
- VOR gülü `NavRecord::mVariation` ile dönüyor; bu değer SDK'nın VOR "heading"
  alanı (slaved variation, + = doğu varsayımı). Log'daki `VOR sample:`
  satırlarıyla doğrulanmadı (Seattle civarı ~+19 beklenir).
- Mod renkleri FAA harita geleneğinden; X-Plane'in kendi haritasıyla
  karşılaştırılıp ayarlanmadı (kullanıcıdan ekran görüntüsü istenebilir).
- IFR Low'da zoom 9'da fix'ler yoğun. MEF rakamları yok. Hava sahası
  sınırları yok.
- FPS ölçümü hiç yapılmadı (Diagnostics modu var).
- Bilinen tek üçgenlenemeyen yüzey: 156.345 yüzeyden 1'i.

**Kullanıcıya sunulan sıradaki iş önerileri (IOS için)** — kullanıcı henüz
seçmedi; önce IOS ekranının X-Plane ile **aynı bilgisayarda mı ayrı
bilgisayarda mı** olduğunu sor (ayrıysa harita ağdan veri alan ayrı bir
uygulama olmalı; mimariyi değiştirir).
1. Uçak bilgi kutusu (irtifa, hız, yön, dikey hız, transponder) + 1/2/5 dk
   tahmin çizgisi + mesafe halkaları
2. Ara / git (ICAO veya navaid kimliği)
3. Cetvel (mesafe/yön ölçme)
4. Haritadan uçak konumlandırma (pist başı, kalkış noktası, serbest nokta)
5. Uçuş kaydı + debrief (geri oynatma, olay işaretleri, CSV/KML)
6. AI / multiplayer trafik
7. Hava durumu katmanı
8. SID/STAR/yaklaşma usulleri (CIFP)
9. Okunurluk: yazı ölçeği, büyük düğmeler, gece teması (dokunmatik hareket
   yok; dokunuşlar fare tıklaması olarak gelir)

## 8. Bilinen tuzaklar (önceki hatalardan)

- `XPLMDrawString` doku açıyor: her geometri çiziminden önce
  `XPLMSetGraphicsState` gerekir (`gfx/Canvas.cpp` bunu kendisi yapar; Canvas
  dışından çizim yapma). GL_CULL_FACE kapalı olmalı.
- Canvas her Draw'da X-Plane'in GL durumunu (program/VAO/VBO/cull) kaydedip
  geri koyar; bunu bozma.
- Boxel ≠ piksel (UI ölçeği): scissor için `GlScreen` kullan.
- Tıklamayı alttaki pencereye geçirme (fare takibi bozuluyor).
- apt.dat'ta sayılar yerel ayardan bağımsız okunmalı (`AptLine`); Türkçe
  Windows'ta `strtod` virgül bekleyebilir.
- Heliport'ların çoğunda düğüm (yüzey) yok, sadece 102 satırı var:
  `FindVisibleAirports(..., pNeedNodes)` bu yüzden parametreli.
- Zemin önbelleği sürümü (`TerrainRaster.cpp` `kFileVersion`) biçim değişince
  artırılmalı; eski dosyalar kendiliğinden yeniden üretilir.
- earcut'taki yama: `third_party/earcut/README.txt`'de belgeli; earcut'ı
  güncellersen yamayı yeniden uygula.
