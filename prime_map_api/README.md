# Prime Map API 0.20 ÔÇö X-Plane 11 harita k├╝t├╝phanesi ve SDK

X-Plane i├ºinde, **kendi penceresinde** ├ºal─▒┼ƒan, katmanlar─▒ tek tek a├º─▒l─▒p
kapat─▒labilen haritan─▒n motoru ve g├Âmme API'si (`prime_map` statik k├╝t├╝phanesi).
D─▒┼ƒ harita servisi (Ka┼ƒif, OSM) yok: zemin, pistler, havaliman─▒ y├╝zeyleri,
navaid'ler, hava yollar─▒ ve u├ºu┼ƒ plan─▒ o bilgisayarda kurulu X-Plane verisinden
gelir. Bu y├╝zden haritada g├Âr├╝nen her ┼ƒey sim├╝lat├Ârdekiyle ayn─▒d─▒r; yeni sahne
kurulunca harita da kendili─ƒinden g├╝ncellenir. Hedef kullan─▒m: sim├╝lat├Ârlerin
e─ƒitmen istasyonu (IOS) ekranlar─▒ ve herhangi bir X-Plane 11 eklentisine g├Âmme.

> Not: Bu repo **k├╝t├╝phane + API + SDK ├╝retimidir**. Resmi X-Plane eklentisi
> (ince kabuk) ayr─▒ repodad─▒r: `prime_map_plugin`.

| Belge | Kimin i├ºin |
|---|---|
| `README.md` (bu dosya) | Kurulum, SDK ├╝retimi, ├╝├º├╝nc├╝ taraf bile┼ƒenler, testler, ara├ºlar |
| `docs/PrimeMap_API.pdf` (kayna─ƒ─▒ `docs/PrimeMap_API.html`) | API'yi kullanacak host geli┼ƒtirici: h─▒zl─▒ ba┼ƒlang─▒├º, fonksiyon referans─▒ |
| `MIMARI.pdf` (kayna─ƒ─▒ `docs/MIMARI.html`) | Kodu anlamak isteyen geli┼ƒtirici: mimari, veri ak─▒┼ƒ─▒ ┼ƒemalar─▒, dosya g├Ârevleri |
| `AGENTS.md` | Projede ├ºal─▒┼ƒacak yapay zek├ó asistan─▒: kurallar, kod standard─▒, derleme, do─ƒrulanm─▒┼ƒ ger├ºekler, durum |

## Prime Map API (0.20)

Harita art─▒k ba┼ƒka bir X-Plane eklentisine g├Âm├╝lebilir bir C++ statik k├╝t├╝phane: `prime_map`
(tek genel ba┼ƒl─▒k `src/api/PrimeMap.h`). Bu repo art─▒k yaln─▒zca k├╝t├╝phaneyi ├╝retir;
resmi X-Plane eklentisi ayr─▒ repodad─▒r (`prime_map_plugin`) ve haritaya sadece bu API ile eri┼ƒir;
kullan─▒c─▒ a├º─▒s─▒ndan g├Âr├╝n├╝┼ƒ ve ayar dosyalar─▒ 0.19 ile ayn─▒. Ayr─▒nt─▒ ve fonksiyon listesi:
`docs/PrimeMap_API.pdf`; b├╝t├╝n fonksiyonlar─▒ deneyen ayr─▒ test eklentisi: `examples/map_api_test`.

Eklentinin men├╝s├╝nden/panellerinden yap─▒lan her ┼ƒeyin API kar┼ƒ─▒l─▒─ƒ─▒ vard─▒r ÔÇö
yeni bir host bu fonksiyonlarla ayn─▒ davran─▒┼ƒ─▒ kurabilir:

| Neyi | API |
|---|---|
| Pencere | `Map::Open`, `SetGeometry`, `SetVisible`, `SetPoppedOut`, `SetUiVisible`, `SetMouseInput` |
| G├Âr├╝n├╝m | `SetCenter`, `SetZoom`, `ZoomIn/Out`, `Pan*`, `FitBounds`, `SetFollowAircraft`, `GoToAirport`, `FindAirports` |
| Mod | `SetMode(mode)`, `NextMode()`, `GetMode()` |
| **Katmanlar** | `SetLayerVisible(MapLayer, bool)`, `ToggleLayer`, `ShowOnlyLayers`, `SetLayersVisible`, `ResetLayersToDefaults`, `GetLayerState` (a─ƒa├º: `TopLevelLayers`, `ParentOf`, `ChildrenOf`) |
| ├çizimler | `SetDrawTool`, `AddDrawingPoint`, `FinishDrawing`, `UndoDrawing`, `Export/ImportDrawings` |
| U├ºak / rota | `SetAircraftSource`, `SetAircraft`, `SetRouteSource`, `SetRoute`, `ClearTrack` |
| Ayarlar | `ExportSettings` / `ImportSettings`, `Get/SetTerrainSettings`, revizyon saya├ºlar─▒ |

## Kurulum ve derleme

**├ûn ko┼ƒul ÔÇö ├╝├º├╝nc├╝ taraf bile┼ƒenler:** a┼ƒa─ƒ─▒daki "├£├º├╝nc├╝ Taraf Bile┼ƒenler"
b├Âl├╝m├╝ndeki bile┼ƒenler `C:\workspace\cots` alt─▒na kurulmu┼ƒ olmal─▒ (repoda
tutulmazlar; ┼ƒirket COTS kural─▒).

Qt Creator'da projeyi a├º ve derle (kit: Desktop Qt 6.10.1 MSVC2022 64bit; Qt
k├╝t├╝phanesi kullan─▒lm─▒yor, sadece MSVC + CMake + Ninja).

Komut sat─▒r─▒ndan:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DMAP_TESTS=ON -DMAP_PREVIEW=ON
ninja -C build
```

- CMake se├ºenekleri: `-DMAP_TESTS=ON` (X-Plane'siz testler, `map_tests.exe`),
  `-DMAP_PREVIEW=ON` (ara├ºlar, a┼ƒa─ƒ─▒da), `-DCOTS_DIR=...` (farkl─▒ COTS klas├Âr├╝),
  `-DXPSDK=...` (farkl─▒ X-Plane SDK klas├Âr├╝).
- Derleme ├º─▒kt─▒lar─▒: `prime_map` statik k├╝t├╝phanesi, `map_tests`, `map_preview`,
  `dsf_info`, ├Ârnek eklenti `examples/map_api_test`.

### SDK paketinin ├╝retimi

Derlemeden sonra kurulum ad─▒m─▒ paketi ├╝retir:

```bat
cmake --install build --prefix C:\workspace\projects\prime_map_sdk
```

Paket i├ºeri─ƒi: `include/PrimeMap.h`, `lib/prime_map_MT.lib` (/MD varyant─▒ ayr─▒
derlemede `-DPRIME_MAP_DYNAMIC_CRT=ON` ile ├╝retilip ayn─▒ prefix'e kurulur),
`docs/PrimeMap_API.pdf`, `CHANGELOG.md`, `examples/map_api_test/`.
Resmi eklentiyi derlemek i├ºin: `prime_map_plugin` reposu (SDK paketini kullan─▒r).

## ├£├º├╝nc├╝ Taraf Bile┼ƒenler

┼×irket kural─▒ gere─ƒi ├╝├º├╝nc├╝ taraf kaynak kodu/binary'leri repoda **tutulmaz**;
`C:\workspace\cots` alt─▒na kurulur ve CMake oradan okur. A┼ƒa─ƒ─▒daki bile┼ƒenleri
indirip ilgili klas├Ârlere a├º─▒n:

| Bile┼ƒen | S├╝r├╝m | Ne i├ºin kullan─▒l─▒yor | ─░ndirme | Kurulum klas├Âr├╝ |
|---|---|---|---|---|
| X-Plane SDK | 3.0 (XPLM301) | X-Plane eklenti aray├╝z├╝: pencere, ├ºizim, dataref, komut ba┼ƒl─▒klar─▒ ve `XPLM_64.lib` | https://developer.x-plane.com/sdk/ | `C:\workspace\cots\X-Plane-SDK` |
| LZMA SDK | 24.05 | DSF dosyalar─▒ 7z ile s─▒k─▒┼ƒt─▒r─▒l─▒r; zemin okuma/yazma | https://www.7-zip.org/sdk.html | `C:\workspace\cots\lzma` |
| earcut.hpp | 2.2.4 | Poligon ├╝├ºgenleme (b├Âlge ├ºizimleri) | https://github.com/mapbox/earcut.hpp | `C:\workspace\cots\earcut` (i├ºi `mapbox/earcut.hpp`) |
| stb_truetype.h | 1.26 | Yaz─▒ atlas─▒ (TrueType okuma) | https://github.com/nothings/stb | `C:\workspace\cots\stb` |

Not: LZMA, earcut ve stb yaln─▒zca **k├╝t├╝phaneyi derlerken** gerekir; SDK
paketiyle host'a teslim edilmez (k├╝t├╝phanenin i├ºine g├Âm├╝l├╝d├╝r). Host'un
gereksinimi sadece X-Plane SDK 3.0'd─▒r (bkz. `docs/PrimeMap_API.pdf`).

Log: `C:\X-Plane 11\Log.txt` i├ºinde `[prime-map-plugin]` ile ba┼ƒlayan sat─▒rlar.

## Kullan─▒m

**Men├╝: Plugins ÔåÆ Prime Map Plugin**

| ├û─ƒe | Komut (Settings ÔåÆ Keyboard'da tu┼ƒa atanabilir) |
|---|---|
| Show / hide map | `prime/map/toggle` |
| Show / hide controls | `prime/map/controls` |
| Next map mode | `prime/map/next_mode` (Scenery ÔåÆ VFR ÔåÆ IFR Low ÔåÆ IFR High ÔåÆ Dark ÔåÆ ...) |
| Show / hide X-Plane map (compare) | X-Plane'in `sim/map/show_current` komutu: yan yana kar┼ƒ─▒la┼ƒt─▒rmak i├ºin |
| Diagnostics mode | `prime/map/diagnostics` |
| Clear flight track | ÔÇö |
| Pre-cache terrain of visible area | `prime/map/precache_view`: haritada g├Âr├╝nen alandaki kurulu karolar─▒n zemin ├Ânbelle─ƒini (raster, rak─▒m, vekt├Âr) arka planda haz─▒rlar; ilerleme alt ┼ƒeritte. ├ûnbellek s─▒n─▒r─▒n─▒n %80'ini a┼ƒacaksa ba┼ƒlamaz |
| Cancel terrain pre-cache | `prime/map/precache_cancel` |
