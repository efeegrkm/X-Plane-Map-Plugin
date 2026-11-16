# prime_map_plugin

X-Plane 11 için resmî Prime XMap eklentisi (**ince kabuk**, 0.26). Haritanın motoru ve arayüzü
[prime_map_api](../prime_map_api) kütüphanesindedir (`prime_map.dll`); bu proje yalnızca X-Plane giriş noktalarını, menüyü,
komutları ve kurulumu içerir. Haritaya **sadece** API ile erişir (`#include <prime_map_api/PrimeMap.h>`).

> Eklenti şu an haritayı **LEGACY `WindowStyle::Standalone`** ile açar (haritanın kendi X-Plane penceresi; pop-out
> desteği için). API'nin hedef yolu `WindowStyle::Texture`'dır; eklentinin Texture'a geçişi planlıdır
> (`../prime_map_api/docs/SRS.md`).

```
src/plugin/PluginMain.cpp   X-Plane giris noktalari, menu, prime/map/* komutlari, ana ve ikinci harita
```

## Menü, komutlar ve API eşleşmesi

Menü: **Plugins → Prime XMap Plugin**. Her öğenin API karşılığı vardır (bir host aynı eşleşmeyi kendi arayüzüne kurabilir):

| Komut (Settings → Keyboard) | Menü öğesi | API çağrısı |
|---|---|---|
| `prime/map/toggle` | Show / hide map | `xmap->setVisible(!xmap->isVisible())` |
| `prime/map/controls` | Show / hide controls | `xmap->setIsUiVisible(!xmap->isUiVisible())` |
| `prime/map/next_mode` | Next map mode | `xmap->nextMode()` |
| `prime/map/second_map` | Open / close second map | `manager->addXMap()` → `setName("second")` → … → `initialize()`; kapatırken `removeXMap` |
| `prime/map/diagnostics` | Diagnostics mode | `xmap->setDiagnostics(!xmap->isDiagnosticsOn())` |
| — | Clear flight track | `xmap->clearTrack()` |
| `prime/map/precache_view` | Pre-cache terrain of visible area | `xmap->precacheVisibleArea()` |
| `prime/map/precache_cancel` | Cancel terrain pre-cache | `xmap->cancelPrecache()` |
| (X-Plane'in `sim/map/show_current`) | Show / hide X-Plane map (compare) | — |

Haritanın kendi arayüzü (üst şerit: Layers, Map modu, Follow aircraft, Go to airport, Draw tools, Pop out, Hide; alt şerit:
konum, zoom, ölçek, durum) ve fare kullanımı: `../prime_map_api/docs/PrimeMap_API.pdf`.

## Dosyalar

Dosyaları kütüphane yazar (eklenti dosya okumaz / yazmaz); veri klasörü kütüphanenin varsayılanıdır:
`%LOCALAPPDATA%\prime\prime-xmap\` (Log'daki `Prime Map API ... ready (data: ...)` satırı tam yolu söyler).

| Dosya | İçerik |
|---|---|
| `prime_map.json` | Merkezi ayarlar: önbellek sınırları, önceden yükleme, otomatik kayıt |
| `maps/main/map_settings.json`, `terrain_colors.json`, `map_drawings.txt` | Ana haritanın ayarları, zemin ayarları, çizimleri |
| `maps/second/...` | İkinci harita (menüden açılırsa) |
| `cache/` | Ortak zemin / havalimanı önbelleği (silinebilir) |

Biçim yalnızca JSON; eski sürümlerin INI dosyaları okunmaz.

## Derleme ve kurulum

Kütüphane bu projenin kardeşi olmalı (`../prime_map_api`; farklı yerdeyse `-DPRIME_MAP_DIR=<klasor>`). Üçüncü taraf
bileşenler: `../prime_map_api/README.md`.

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build_cmd -G Ninja -DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe -DCMAKE_BUILD_TYPE=Release -DMAP_DEPLOY=ON
ninja -C build_cmd
```

`MAP_DEPLOY=ON` (varsayılan) derleme sonrası `win.xpl` + `prime_map.dll`'yi birlikte
`<X-Plane>/Resources/plugins/prime-map-plugin/64/` altına kopyalar (X-Plane açıkken dosyalar kilitli). Log.txt'de
`[prime-map-plugin] Prime Map API 0.26.0 ready` ve `loaded (version 0.26, API 0.26.0)` görünmelidir. Hedef makinede
`VC_redist.x64` (MSVC 2022) gerekir.

| Belirti | Neden / çözüm |
|---|---|
| CMake: `prime_map_api bulunamadi` | Kardeş klasör yok ya da `-DPRIME_MAP_DIR` yanlış |
| X-Plane'de harita yok, Log'da DLL hatası | `prime_map.dll` `64/`'da yok ya da bayat; yeniden derleyin |
| Aynı süreçte başka bir host eklentisi de var | Hepsi aynı `prime_map.dll` sürümünü kullanmalı (Windows aynı adlı DLL'i bir kez yükler) |
