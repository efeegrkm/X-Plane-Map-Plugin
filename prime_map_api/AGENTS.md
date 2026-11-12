# AGENTS — Prime Map (asistanlar için durum ve kurallar)

Durum tarihi: **2026-11-12**, sürüm **0.23.0**. Kullanıcıyla **Türkçe** konuş; kısa ve somut ol
(gereksiz deneme / uzun rapor istemiyor). Kod yorumları Türkçe + ASCII, arayüz / log / commit İngilizce.

## Proje

**Prime Map**, X-Plane 11 haritasını başka bir X-Plane eklentisinin içine gömen C++20 kütüphanesidir
(`prime_map.dll`, SHARED, namespace `prime`). Veri sadece X-Plane'den (DSF, apt.dat, nav, earth_awy, dataref).

- `prime_map_api/` — kütüphane. Tek genel başlık `PrimeMap.h` (kök). Giriş: `prime::MapLibrary::Get()`.
- `prime_map_plugin/` — resmî eklenti (ince kabuk; haritaya sadece API ile erişir).
- `plugin-api-test/` — API test eklentisi (+ `MultiMapSelfTest`: X-Plane içi otomatik test).
- Belgeler: `docs/PrimeMap_API.html/pdf` (API, eksiksiz tutulmalı), `docs/MIMARI.html/pdf` (mimari; yeni iş
  13c bölümünde), `docs/CHANGELOG.md`, `README.md`, `docs/ZEMIN_HIZ_RAPORU.md`.

## 0.23'te yapılan (görev tamam, X-Plane'de DENENMEDİ)

Harita fabrikası + tamamen bağımsız çoklu harita + harita başına ayar dosyaları + merkezi ayarlar +
ortak önbellek. Özet (ayrıntı MIMARI 13c, API belgesi):

- `MapLibrary::CreateMap()` pencere açmadan nesne verir; her ayar `Open()`'dan önce de sonra da verilir.
- `engine/MapData` = ortak veri / önbellek / flight loop; `engine/MapInstance` = bir haritanın her şeyi
  (LayerManager + katmanlar, Drawings, AircraftData, palet, MapWindow, dosyalar).
- `layers/TerrainHub` = dokular ve vektörler haritalar arası ortak (tek GPU kopyası).
- Dosyalar: `<veri>/prime_map.ini`, `<veri>/maps/<ad | map_<kimlik>>/{map_settings.ini, map_drawings.txt,
  terrain_colors.ini}`, `<veri>/cache/`. Veri klasörü varsayılan host eklenti klasörü.
- Öncelik: `LoadAndSave`'de `Open` dosyayı kodla verilenin üstüne okur; `SaveOnly` okumaz; `Off` dosyasız.
- Sınır: `taban × (1 + artış% × (açık harita − 1) / 100)`, `prime_map.ini [cache] / [memory]`.

Kullanıcı X-Plane testini kendisi yapacak: liste `README.md` "Test listesi (0.23)". Sonuç gelmeden
"çalışıyor" deme.

## Sıradaki olası işler (kullanıcı seçmeden başlama)

1. Zemin hızı: `docs/ZEMIN_HIZ_RAPORU.md` bölüm 5'teki sıra (işçi önceliği, önce seviye 1, rakım
   sıkıştırmasını erteleme; hazır görüntü önbelleği; paralel karo üretimi; su şekilleri / dünya özeti).
2. `examples/map_api_test` eski (0.20 testi; sürüm ve tek harita beklentileri bayat): güncelle ya da kaldır.
3. Katman paneline zoom aralığı arayüzü yok (sadece API / dosya).

## Kod standardı (kesin)

Önekler `m` üye, `p` parametre, `t` yerel, `k` sabit, `g` global. Süslü parantez her zaman alt satırda
(lambda dahil), tek satır `if` de parantezli. Tür / fonksiyon PascalCase, satır ≤ 120. C++20, `/W4` uyarısız.
XPLM callback'lerine refcon `this`; global durum yok. Çekirdek (`CORE_SOURCES`) X-Plane'e bağımlı değil
(testlerde çalışır). GL çağrısı sadece `src/gfx/`. Arka plan thread'inde XPLM / `Log` yok.

## Bu bilgisayarda yerleşim ve git (ev: `C:\Projects\QT`)

```
C:\Projects\QT\prime_map_api      C:\Projects\QT\prime_map_plugin      C:\Projects\QT\plugin-api-test
C:\Projects\QT\cots               üçüncü taraf (X-Plane-SDK, lzma, earcut, stb); CMake C:\workspace\cots yoksa burayı bulur
C:\Projects\QT\README.md, .gitignore   deponun kök dosyaları
```

- Depo: `https://github.com/efeegrkm/X-Plane-Map-Plugin` (`main`). `.git` klasörü
  `prime_map_plugin\.git`'tedir ve `core.worktree = C:/Projects/QT`; `prime_map_api` ve `plugin-api-test`
  içinde ona işaret eden `.git` dosyası var. Üç klasörün herhangi birinden normal `git` komutları çalışır.
- **DİKKAT:** çalışma ağacı `C:\Projects\QT` olduğu için oradaki diğer projeler `.git/info/exclude` ile
  yok sayılır. **`git clean` ÇALIŞTIRMA** (`-x` / `-fd` diğer projeleri silebilir); `git add -A` yerine
  `git add -A :/prime_map_api :/prime_map_plugin :/plugin-api-test :/README.md` kullan.
- Kullanıcı bu depoya doğrudan push'a izin verdi (iş bilgisayarındaki stage22 / robocopy akışı burada geçerli değil).
- `prime_map_plugin\build\` ve `.qtcreator\` kullanıcınındır: dokunma. `prime_map_plugin\Claude outputs\` izlenmez.

## Derleme / test (bu bilgisayar)

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
C:\Qt\Tools\CMake_64\bin\cmake.exe -S C:\Projects\QT\prime_map_api -B <derleme> -G Ninja ^
  -DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe -DCMAKE_BUILD_TYPE=Release -DMAP_TESTS=ON -DMAP_PREVIEW=ON
C:\Qt\Tools\Ninja\ninja.exe -C <derleme>
<derleme>\map_tests.exe        REM 30682 checks passed, 0 failed
<derleme>\map_preview.exe --lat 47.45 --lon -122.30 --zoom 11 --mode scenery --cache <klasor>\ --out ksea.ppm
```

Eklentiler aynı şekilde (`-S ...\prime_map_plugin`, `-S ...\plugin-api-test`); `-DMAP_DEPLOY=ON`
`win.xpl` + `prime_map.dll`'yi X-Plane'e kopyalar (resmî eklentide varsayılan AÇIK, test eklentisinde KAPALI;
X-Plane açıkken dosya kilitli). Kendi derlemeni geçici klasörde yap. PDF:
`chrome --headless=new --disable-gpu --no-pdf-header-footer --print-to-pdf=docs\X.pdf docs\X.html`.
Bash heredoc kaynak dizgilerini bozabiliyor: çok satırlı düzenlemeyi dosyaya yazılmış Python betiğiyle yap.

## Tuzaklar

- Derleme ağacındaki `64\` ara kopyası bayat kalabilir; kurulumu `prime_map_build\prime_map.dll` üzerinden doğrula.
- Sürüm: `PrimeMap.h` `kVersion*` ile CMake `project(VERSION)` configure'da denetlenir; birlikte değiştir.
  Eklentinin `kVersion` sabiti ve CMake sürümü de aynı olmalı.
- Pop-out penceresi ayrı GL bağlamıdır: VAO paylaşılmaz (`gfx/ContextVertexArray`), doku / buffer paylaşılır
  (TerrainHub buna dayanır).
- Yeni bir "harita başına" durum eklersen: `MapInstance` + `map_settings.ini` (MapWindow::SaveSettings /
  ApplySettings ya da MapInstance::StoreExtraSections) + API belgesi bölüm 8.
- Yeni görünürlük durumu: `LayerManager::VisibilitySignature` / `MapWindow::SceneSignature`'a kat.
- Zemin önbellek biçimi değişirse `TerrainRaster.cpp` / `TerrainElevation.cpp` `kFileVersion`'ı artır.
- `TerrainStore`'un bellek LRU'ları sadece işçi thread'inindir (kilitsiz); ana thread'den dokunma.
- Kaspersky taze derlenmiş exe'leri (dsf_info, map_preview) yanlış alarmla karantinaya alabilir: yeniden derle.
- Doğrulanmış veri biçimi gerçekleri (apt.dat, DSF, earth_awy) kodda `GERCEK VERI` notlarıyla; MIMARI'de özet.
