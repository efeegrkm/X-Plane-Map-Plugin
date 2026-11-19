# prime_map_plugin

Prime XMap'in resmî X-Plane 11 eklentisi. Sürüm 1.1.1.

Haritanın bütün işlevi `prime_map_api` kütüphanesindedir. Bu proje yalnızca X-Plane giriş noktalarını, menüyü ve komutları içerir ve haritaya yalnızca genel API ile erişir. Eklenti haritayı kendi X-Plane penceresinde (`WindowStyle::Standalone`) açar.

## Menü ve komutlar

Menü: **Plugins → Prime XMap Plugin**.

| Komut | Menü öğesi | API çağrısı |
|---|---|---|
| `prime/map/toggle` | Show / hide map | `setVisible` |
| `prime/map/controls` | Show / hide controls | `setIsUiVisible` |
| `prime/map/next_mode` | Next map mode | `nextMode` |
| `prime/map/second_map` | Open / close second map | `addXMap`, `initialize`, `removeXMap` |
| `prime/map/diagnostics` | Diagnostics mode | `setDiagnostics` |
| — | Clear flight track | `clearTrack` |
| `prime/map/precache_view` | Pre-cache terrain of visible area | `precacheVisibleArea` |
| `prime/map/precache_cancel` | Cancel terrain pre-cache | `cancelPrecache` |

## Dosyalar

Eklenti kütüphanenin varsayılan veri klasörünü kullanır: `%LOCALAPPDATA%\prime\prime-xmap\`. Ana harita `maps/main/`, ikinci harita `maps/second/` altında saklanır. `Log.txt`'deki `Prime Map API 1.1.1 ready (data: ...)` satırı klasörün tam yolunu gösterir.

## Derleme ve kurulum

`prime_map_api` bu projenin kardeş klasöründe olmalıdır; başka bir yerdeyse `-DPRIME_MAP_DIR` ile verilir. Üçüncü taraf bileşenler için `prime_map_api/README.md` dosyasına bakın.

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DMAP_DEPLOY=ON
cmake --build build
```

`MAP_DEPLOY=ON` (varsayılan) derleme sonunda `win.xpl` ve `prime_map.dll` dosyalarını `<X-Plane>/Resources/plugins/prime-map-plugin/64/` klasörüne kopyalar. X-Plane açıkken bu dosyalar kilitli olduğu için kopyalama öncesinde X-Plane kapatılmalıdır.

Aynı X-Plane'de `prime_map.dll` kullanan başka eklentiler varsa hepsi aynı kütüphane sürümüyle derlenmelidir.
