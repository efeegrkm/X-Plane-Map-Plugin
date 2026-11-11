# prime_map_plugin

X-Plane 11 için harita eklentisi (**ince kabuk**): haritanın motoru ve arayüzü
[prime_map_api](../prime_map_api) projesinin SHARED kütüphanesinde
(`prime_map.dll`) durur; bu proje sadece X-Plane giriş noktasını
(`src/plugin/PluginMain.cpp`), ayar dosyası okuma/yazmasını (`Settings`) ve
kurulumu içerir.

## Yapı

```
src/plugin/PluginMain.cpp   X-Plane giriş noktaları, menü, komutlar, ayar dosyaları
src/Settings.h/.cpp         map_settings.ini okuma/yazma yardımcısı (kopya; motor tarafında da var)
```

Haritaya **sadece** `PrimeMap.h` (API) üzerinden erişilir:
`#include <prime_map_api/PrimeMap.h>`.

## Menü, komutlar ve API eşleşmesi

Eklentide "eklentiye özel" tek şey X-Plane tarafı: menü, klavye komutları ve
ayar dosyaları. Her biri API çağrısına çevrilir — eklentide yapılabilen her şey
API'de de vardır (yeni bir host aynı eşleşmeyi kendi UI'ına kurabilir):

| Komut (Settings → Keyboard) | Menü öğesi | Yaptığı API çağrısı |
|---|---|---|
| `prime/map/toggle` | Show / hide map | `map->SetVisible(!IsVisible())` |
| `prime/map/controls` | Show / hide controls | `map->SetUiVisible(!IsUiVisible())` |
| `prime/map/diagnostics` | Diagnostics mode | `map->SetDiagnostics(!IsDiagnosticsOn())` |
| `prime/map/next_mode` | Next map mode | `map->NextMode()` |
| `prime/map/precache_view` | Pre-cache terrain | `map->PrecacheVisibleArea()` |
| `prime/map/precache_cancel` | Cancel pre-cache | `map->CancelPrecache()` |

Haritanın geri kalan kontrolü de API'dedir (eklenti menüsünde ayrı öğe olarak
dursun dursun API'de karşılığı vardır): görünüm için `SetCenter`, `SetZoom`,
`PanLeft/Right/Up/Down`, `ZoomIn/ZoomOut`, `FitBounds`, `SetFollowAircraft`,
`GoToAirport`; mod için `SetMode(mode)` / `NextMode()`; **katmanlar için
`SetLayerVisible(MapLayer, bool)`, `ToggleLayer`, `ShowOnlyLayers`,
`ResetLayersToDefaults`** (katman listesi ve kurallar:
`docs/PrimeMap_API.pdf` böl. 6–7); çizim için `SetDrawTool`,
`AddDrawingPoint`, `FinishDrawing`, `UndoDrawing`; uçak/rota için
`SetAircraftSource`, `SetRouteSource` + Host kaynakları. Ayarlar:
`ExportSettings` / `ImportSettings` (+ revizyon sayaçları). Bütün liste ve
açıklamalar `docs/PrimeMap_API.pdf`'tedir.

## Üçüncü Taraf Bileşenler

Şirket kuralı gereği üçüncü taraf bileşenler repoda tutulmaz;
`C:\workspace\cots` altından okunur:

| Bileşen | Sürüm | Ne için kullanılıyor | İndirme | Kurulum klasörü |
|---|---|---|---|---|
| X-Plane SDK | 3.0 (XPLM301) | X-Plane eklenti arayüzü: başlıklar + `XPLM_64.lib` | https://developer.x-plane.com/sdk/ | `C:\workspace\cots\X-Plane-SDK` |

## Gereksinim: prime_map_api

Derlemeden önce kütüphane reposu gerekir (SDK paketi **yoktur**; tüketim
`add_subdirectory` + hedef adına link yoluyladır). Varsayılan beklenti: bu klasörün
kardeşi olan `../prime_map_api`. Farklı yerdeyse: `-DPRIME_MAP_DIR=<klasor>`.

## Derleme

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build_cmd -G Ninja -DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe ^
      -DCMAKE_BUILD_TYPE=Release -DMAP_DEPLOY=ON
ninja -C build_cmd
```

`MAP_DEPLOY=ON` (varsayılan) derleme sonrası `win.xpl`'yi **ve** `prime_map.dll`'yi
`<X-Plane>/Resources/plugins/prime-map-plugin/64/` altına kopyalar
(X-Plane açıkken dosya kilitli; önce kapat).

CRT: `/MD` (kütüphaneyle aynı). Hedef makinede MSVC 2022
redistributable kurulu olmalıdır.

## Dosyalar (eklenti klasöründe)

| Dosya | İçerik |
|---|---|
| `map_settings.ini` | pencere, görünüm, önbellek, katmanlar (host yazar) |
| `map_drawings.txt` | kullanıcı çizimleri |
| `terrain_colors.ini` | zemin renk ayarları |
| `cache/` | zemin/havalimanı önbelleği |
