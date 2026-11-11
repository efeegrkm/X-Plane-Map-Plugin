# AGENTS — Prime Map (ajana görev devri)

> Bu dosya sonraki otomatik çalışma için **görev devri** belgesidir (2026-11-11).
> Eski kurallar temizlendi; aşağıda projenin durumu, çalıştırma komutları ve
> yapılacak işler var.

## Proje nedir

**Prime Map**, X-Plane 11 haritasını başka bir X-Plane eklentisinin içine gömen
C++20 kütüphanesidir (`prime_map.dll`, SHARED, namespace `prime`).

- `prime_map_api/` — kütüphane (bu repo). Tek genel başlık: `PrimeMap.h`
  (repo kökü). Giriş noktası: `prime::MapLibrary::Get()` — `Init(config)`,
  `CreateMap(openConfig)` (birden çok harita açılabilir), `Shutdown()`
  (referans sayaçlı), `GetStatus()`.
- `prime_map_plugin/` — resmi X-Plane eklentisi (haritaya sadece API ile erişir).
- `plugin-api-test/` — tüm API çağrılarını deneyen test eklentisi.
- Tüketim modeli: `add_subdirectory(<prime_map_api> prime_map_build)` +
  `target_link_libraries(<hedef> PRIVATE prime_map)`; `prime_map.dll` host'un
  `win.xpl` dosyasının yanına kopyalanır (`POST_BUILD` kopya kuralı var).
- Dokümanlar: `prime_map_api/docs/` (README.md, PrimeMap_API.html/pdf,
  MIMARI.html/pdf, CHANGELOG.md).

## Versiyon kontrolü (kesin kural)

- `C:\workspace\projects\*` klasörleri **hiçbir repoya bağlı değildir**; git
  yalnızca `%TEMP%\opencode\vc\X-Plane-Map-Plugin` klonunda yaşar
  (GitHub: `https://github.com/efeegrkm/X-Plane-Map-Plugin`, `main` dalı).
- Her anlamlı değişiklik: `stage22.ps1` ile üç projeyi
  `%TEMP%\opencode\sdk22`'ye kopyala → klonda `git checkout main` →
  `git rm -r -q .` → `robocopy %TEMP%\opencode\sdk22 <klon> /E` →
  `git add -A` → `git commit -F <mesaj-dosyasi.txt>` (çok satırlı mesaj
  dosyasından; PS `-m` çok satırı bozar) → `git push origin main`.
- Repo kök dizin yapısı: `prime_map_api/`, `prime_map_plugin/`,
  `plugin-api-test/`, `AGENTS.md` (bu dosya).

## Derleme / test

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
set PATH=C:\Qt\Tools\Ninja;%PATH%

cmake --build C:\workspace\projects\prime_map_api\build\a22
C:\workspace\projects\prime_map_api\build\a22\map_tests.exe     REM 30.659 kontrol / 0 hata beklenir
cmake --build C:\workspace\projects\plugin-api-test\build\t22
cmake --build C:\workspace\projects\prime_map_plugin\build\p22
```

X-Plane kurulumu (X-Plane KAPALIYKEN; `win.xpl` + `prime_map.dll` birlikte):

```
C:\workspace\projects\plugin-api-test\build\t22\prime_map_build\prime_map.dll
     -> C:\X-Plane 11\Resources\plugins\plugin-api-test\64\prime_map.dll
C:\workspace\projects\plugin-api-test\build\t22\plugin_api_test\64\win.xpl
     -> C:\X-Plane 11\Resources\plugins\plugin-api-test\64\win.xpl
C:\workspace\projects\prime_map_plugin\build\p22\prime_map_build\prime_map.dll
     -> C:\X-Plane 11\Resources\plugins\prime-map-plugin\64\prime_map.dll
C:\workspace\projects\prime_map_plugin\build\p22\prime-map-plugin\64\win.xpl
     -> C:\X-Plane 11\Resources\plugins\prime-map-plugin\64\win.xpl
```

**Tuzaklar (iki kez yakıldık):**
- Derleme ağacındaki `64\` ara kopyası bayat kalabilir; kurulumu **daima**
  `prime_map_build\prime_map.dll` (bağlama çıktısı) üzerinden yap ve binary
  içinde beklenen dizenin olup olmadığını doğrula.
- `prime_map_api\build\a22` CMake cache'i bayatlayabilir (örn. `MAP_TESTS=OFF`
  kalabilir); şüphede taze dizine configure et.
- `rebuild_all.bat` hataları `findstr /V` ile gizler; gerçek hatayı görmek için
  doğrudan `cmake --build` çalıştır.

## Yapılacak iş (görev)

### Görev 1 — Çoklu harita: her harita bağımsız gezilebilmeli (ASIL İŞ)

**Durum (0.22, güncel):** "tek harita" kuralı kaldırıldı; `MapLibrary::CreateMap`
ile aynı anda birden çok `Map` penceresi açılabiliyor. Ama tüm pencereler
**ortak** `MapData` üzerinde çalışıyor ve her pencere aynı `MapView`'ı değil,
**kendi** `MapView`'ını kullanıyor (bkz. `MapWindow::mView`) — yani kamera
pencere başına. ORTAK olanlar: katmanlar (`LayerManager`), uçak verisi,
kullanıcı çizimleri, havalimanı indeksi, zemin paleti, durum metni.

**İstenen davranış:** her haritada farklı yere gidilebilmeli — bir haritada
KSEA'ya zoom yaparken diğerinin görünümü değişmemeli; `SetCenter` / `SetZoom` /
pan / `GoToAirport` / `SetFollowAircraft` pencereye özel olmalı.

**Mevcut kodda neye bakılmalı:**
- `prime_map_api/src/api/PrimeMap.cpp` — `Map::Impl` artık kendi `MapWindow`'unu
  tutuyor (`mWindow`); görünümlü tüm çağrılar pencere üzerinden gider.
- `prime_map_api/src/engine/MapData.cpp` — `mWindows` listesi (çoklu pencereyi
  besler); `AddWindow/RemoveWindow`, `Update` döngüsü, `UpdateStatus` (yayın),
  `UpdatePrefetch` (herhangi bir pencere görünürse).
- Kamera durumu `MapWindow::mView`'da (`MapView`); pencere başına zaten var.
  Paylaşımı bozan noktalar: pencere içi ayarlar `Settings mSettings` (mod
  başına katmanlar, görünüm) — ikinci pencere açılırken varsayılanlarla mı
  başlıyor, ilk pencereden mi kopyalanıyor kontrol et.
- `plugin-api-test`'in `OpenSecondMap` düğmesi ikinci haritayı açar (10,300
  konumunda 200x150): bağımsız gezinmeyi bununla doğrula.
- Test senaryosu: iki harita aç → harita 1'i KSEA'ya götür → harita 2'nin
  görünümü DEĞİŞMEMELİ → harita 2'yi Paris'e götür → harita 1 aynı kalmalı.

### Görev 2 — Runway/zoom (bitti, doğrula)

Pistler artık zoom 11'de görünüyor (pavement eşiğiyle aynı; eskiden 9'du).
`RunwayLayer.cpp` `kMinZoom` + `RunwayLayer.h` yorumu. X-Plane'de uzak zoom'da
pistlerin kaplamalarla birlikte kaybolduğunu görsel olarak doğrula.

### Görev 3 — Yapılmış olanları teyit et (kısa)

- Namespace `prime` (eski `PrimeMap` yerine) — kod + doküman tamam.
- `Init` artık nesne üstünde: `prime::MapLibrary::Get().Init(...)` (mentor
  geri bildirimi; serbest `Init/Shutdown` fonksiyonları kaldırıldı).
- Çoklu host: `Init` idempotent + referans sayaçlı (`hosts: N` log'u).
- Log'da görülmesi gerekenler: `Prime Map API 0.22.0 ready (cache: ...)`,
  iki hostta `Init: library already running (hosts: 2)`,
  `loaded (version 0.22, API 0.22.0)`; `called twice` / `only one map` / tek
  harita uyarıları YOK.

## Sürüm kuralları

- `PrimeMap.h` içindeki `kVersionMajor/Minor/Patch` (0.22.0) ile CMake
  `project(VERSION 0.22)` configure sırasında denetlenir; değiştirirken ikisini
  birlikte güncelle. Davranış değişikliği bitince patch yükseltmeyi düşün
  (ör. 0.22.1) ve `docs/CHANGELOG.md`'ye madde ekle.
