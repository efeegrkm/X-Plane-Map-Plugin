# AGENTS — Prime XMap (asistanlar için durum ve kurallar)

Durum tarihi: **2026-11-17**, sürüm **0.26.0**. Kullanıcıyla **Türkçe** konuş; kısa ve somut ol (gereksiz deneme / uzun
rapor istemiyor). Kod yorumları Türkçe + ASCII, arayüz / log / commit İngilizce.

`docs/CHANGELOG.md`, `ZEMIN_HIZ_RAPORU.md`, `GECIS_0.22_RAPOR.md`, `MIMARI.html`, `PrimeMap_API.html` **yereldir**
(`.gitignore`). Repoda ayrıca: README'ler, `docs/SRS.md`, `docs/MIMARI.pdf`, `docs/PrimeMap_API.pdf`,
`docs/tools/build_api_doc.py` (API belgesinin KAYNAĞI: HTML'i bu üretir, örnek kodu derlenen dosyadan okur).

## İlkeler (kullanıcı kararları)

- **Hedef:** API ile haritanın her yerinde tam kontrol (SRS bölüm 1.1). Yeni iş SRS'teki bir gereksinime bağlanır;
  bitince SRS durumu (K / U / KS / Y / H) ve kılavuz güncellenir.
- **Tek hedef yol `WindowStyle::Texture`** (framebuffer). `Standalone` ve `Embedded` **LEGACY**: Texture kapsamlı denenene
  kadar KALDIRMA, sadece "LEGACY" diye işaretli tut. Yeni host kodu Texture kullanır.
- **Geriye uyumluluk yok** (geliştirme aşaması, kullanılan ürün yok): uyumluluk / göç kodu yazma. Ayarlar yalnızca JSON.
- Gereksizi kaldır, çöp biriktirme. X-Plane'de denenmemiş şeye "çalışıyor" deme.

## Proje

- `prime_map_api/` — kütüphane (CMake hedefi `prime-map-api`, `prime_map.dll`, namespace `prime`). Toplama başlık
  `PrimeMap.h` → `src/api/{XMapManagerFactory, XMapManager, XMap, XMapTypes}.h`. Motor: `engine/XMapData` (ortak veri,
  flight loop), `XMapWindow` (bir haritanın çizimi + girdi; Texture ve LEGACY pencere yolu), `engine/LayerTable`.
  `examples/texture_host/` en küçük tam host (kılavuz bölüm 3'ün kodu; derlenmeli).
- `prime_map_plugin/` — resmî eklenti (LEGACY Standalone; dosya göçü yok).
- `plugin-api-test/` — test eklentisi: taşınır / boyutlanır pencerede Texture harita, `XMapApiCalls` (her çağrı ayrı),
  `MultiXMapSelfTest` (eklenti klasöründe `selftest.flag` varsa; LEGACY Embedded kullanır). Kendi veri klasörü.

## API özeti

```cpp
prime::XMapManager* tManager = prime::XMapManagerFactory::createXMapManager();
tManager->setDataDirectory(dir); tManager->initialize();
prime::XMap* tMap = tManager->addXMap(tConfig);           // tConfig.mStyle varsayilan Texture
tMap->setTextureSize(w, h); tMap->initialize();
// draw callback: if (tMap->renderTexture()) XPLMBindTexture2d(tMap->getTextureId(), 0) + dortgen
// girdi: injectMouse / injectMouseMove / injectWheel / injectKey (doku koordinati: sol-alt orijin, boxel)
tManager->removeXMap(tMap); tManager->shutdown(); prime::XMapManagerFactory::destroyXMapManager(tManager);
```

Dosyalar (veri klasörü `setDataDirectory` ya da `%LOCALAPPDATA%/prime/prime-xmap`): `prime_map.json`,
`maps/<ad>/{map_settings.json, terrain_colors.json, map_drawings.txt}`, `cache/`.

## Bilinen sorunlar (SRS Ek B)

- `setColor(TerrainBuiltUp)` `true` döner ama görünmez (zemin dokusu modun sabit rengini kullanır).
- `shutdown()` sonrası `XMap` metotları korumasız (`mWindow` boş): host önce `removeXMap` yapmalı.
- Log öneki süreç genelinde tek (`Log.cpp` global).
- `map_tests`: 1 s'lik arka plan bekleme döngüleri (tests.cpp ~1380 / 1486 / 2576) yük altında nadiren düşüyor.
- Texture, pop-out edilmiş host penceresinde çizemez (ayrı GL bağlamı).
- `XMap.h` iç başlıkları dışa açıyor (C4251 sınıf boyunca kapalı); pimpl yok.

## Devam planı (SRS 5.2; kullanıcı seçmeden başlama)

1. **Texture kabul testleri** (SRS bölüm 4 tablosu; kullanıcı X-Plane'de yapar) → U → K, bulunan hatalar.
2. **Stil modeli**: katman başına renk / kalınlık / boy / yazı (`XMapStyle`), mod başına, tema = stil kümesi; havalimanı,
   yüzey, nesne, uçak, rota, iz, çizim, etiket, arka plan renkleri; `TerrainBuiltUp` hatası (STY-01..07, MODE-02).
3. **Zoom modeli**: yerleşik eşikleri (katman .cpp'lerindeki `k...MinZoom`) tabloya taşı, iki yönlü ayarlanabilir yap
   (ZOOM-02/03).
4. **Host katmanları**: host'un sembol / çizgi / alan / etiket ekleyebildiği katman API'si (LAY-04, DRW-03).
5. **Olaylar ve yapısal sorgu** (INP-06, QRY-02).
6. **Texture pop-out + hazır pencere yardımcısı**; resmî eklentiyi Texture'a geçir; sonra LEGACY stilleri kaldır
   (EMB-06..08).
7. Pimpl, `shutdown` sonrası güvenli no-op, yönetici başına Log öneki (ARY-03, LIF-08, NF-REL-04).
8. Performans ölçümü ve zemin hızı (`docs/ZEMIN_HIZ_RAPORU.md` bölüm 5).

## Kod standardı (kesin)

Önekler `m` üye, `p` parametre, `t` yerel, `k` sabit, `g` global. **Fonksiyon / metot camelCase**, tür PascalCase. Süslü
parantez her zaman alt satırda (lambda dahil), tek satır `if` de parantezli. Satır ≤ 120, C++20, `/W4` uyarısız.
XPLM callback'lerine refcon `this`; global durum yok. Çekirdek X-Plane'e bağımlı değil (testlerde çalışır). GL çağrısı
sadece `src/gfx/`. Arka plan thread'inde XPLM / `Log` yok.

## Bu bilgisayarda yerleşim ve git (ev: `C:\Projects\QT`)

```
C:\Projects\QT\prime_map_api      C:\Projects\QT\prime_map_plugin      C:\Projects\QT\plugin-api-test
C:\Projects\QT\cots               üçüncü taraf (X-Plane-SDK, lzma, earcut, stb); CMake C:\workspace\cots yoksa burayı bulur
C:\Projects\QT\README.md, .gitignore   deponun kök dosyaları
```

- Depo: `https://github.com/efeegrkm/X-Plane-Map-Plugin` (`main`). `.git` klasörü `prime_map_plugin\.git`'tedir,
  `core.worktree = C:/Projects/QT`; üç klasörün herhangi birinden `git` çalışır.
- **DİKKAT:** çalışma ağacı `C:\Projects\QT`. **`git clean` ÇALIŞTIRMA**; `git add -A` yerine
  `git add -A :/prime_map_api :/prime_map_plugin :/plugin-api-test :/README.md`.
- Pull'da izlenmeyen yerel belgeler silinebilir (izlemeden çıkaran commit gelirse): önce yedekle, sonra geri koy.
- Kullanıcı bu depoya doğrudan push'a izin verdi.
- `prime_map_plugin\build\` ve `.qtcreator\` kullanıcınındır: dokunma. `prime_map_plugin\Claude outputs\` izlenmez.
- X-Plane'de kurulu: `prime-map-plugin`, `plugin-api-test` (ikisinde birebir aynı `prime_map.dll` olmalı). Eski 0.20
  `map_api_test` kaldırıldı.

## Derleme / test (bu bilgisayar)

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
C:\Qt\Tools\CMake_64\bin\cmake.exe -S C:\Projects\QT\prime_map_api -B <derleme> -G Ninja ^
  -DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe -DCMAKE_BUILD_TYPE=Release -DMAP_TESTS=ON -DMAP_PREVIEW=ON
C:\Qt\Tools\Ninja\ninja.exe -C <derleme>
<derleme>\map_tests.exe        REM 30682 checks passed, 0 failed (2026-11-17)
```

Eklentiler ve örnek aynı şekilde (`-S ...\prime_map_plugin`, `-S ...\plugin-api-test`,
`-S ...\prime_map_api\examples\texture_host`); `-DMAP_DEPLOY=ON` X-Plane'e kopyalar (resmî eklentide varsayılan AÇIK;
X-Plane açıkken dosya kilitli). Kendi derlemeni geçici klasörde yap. Dört proje de 0 uyarı olmalı.

Belgeler: `python docs/tools/build_api_doc.py` → `docs/PrimeMap_API.html`; PDF (MIMARI de aynı):
`chrome --headless=new --disable-gpu --no-pdf-header-footer --print-to-pdf=docs\X.pdf docs\X.html`.
Bash heredoc kaynak dizgilerini (ters eğik çizgi) bozar: çok satırlı düzenlemeyi dosyaya yazılmış Python betiğiyle yap.

## Hata ayıklama

- X-Plane çökmesi: `C:\X-Plane 11\Output\crash_reports\*.dmp`. Hata ayıklayıcı yok; minidump'ı Python ile oku
  (istisna kodu, adres, yığındaki modül+ofset), aynı derlemeyi `-DCMAKE_SHARED_LINKER_FLAGS=/MAP` ile tekrar üretip
  `.map`'ten fonksiyon adını bul (kod yerleşimi aynı kalır).

## Tuzaklar

- Sürüm: `src/api/XMapTypes.h` `kVersion*` + CMake `project(VERSION)` + eklenti `kVersion` + öz-test birlikte.
- Pop-out penceresi ayrı GL bağlamıdır: VAO paylaşılmaz (`gfx/ContextVertexArray`), doku / buffer paylaşılır.
- Yeni "harita başına" durum: `XMap` + `map_settings.json` (XMapWindow ayar deposu ya da `XMap::storeExtraSections`)
  + kılavuz (`build_api_doc.py`) + SRS.
- Yeni görünürlük / stil durumu: `LayerManager::visibilitySignature` / `XMapWindow::sceneSignature`'a kat (Texture ve
  resim önbelleği yeniden çizimi buna bakar).
- Zemin önbellek biçimi değişirse `TerrainRaster.cpp` / `TerrainElevation.cpp` `kFileVersion`'ı artır.
- `TerrainStore`'un bellek LRU'ları sadece işçi thread'inindir; ana thread'den dokunma.
- Kaspersky taze derlenmiş exe'leri yanlış alarmla karantinaya alabilir: yeniden derle.
- Doğrulanmış veri biçimi gerçekleri (apt.dat, DSF, earth_awy) kodda `GERCEK VERI` notlarıyla.
