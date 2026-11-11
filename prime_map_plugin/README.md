# prime_map_plugin

X-Plane 11 i├ºin harita eklentisi (**ince kabuk**): haritan─▒n motoru ve aray├╝z├╝
[prime_map_api](../prime_map_api) projesinden ├º─▒kan SDK paketi i├ºindeki statik
k├╝t├╝phanede (`prime_map_MT.lib`) durur; bu proje sadece X-Plane giri┼ƒ noktas─▒n─▒
(`src/plugin/PluginMain.cpp`), ayar dosyas─▒ okuma/yazmas─▒n─▒ (`Settings`) ve
kurulumu i├ºerir.

## Yap─▒

```
src/plugin/PluginMain.cpp   X-Plane giri┼ƒ noktalar─▒, men├╝, komutlar, ayar dosyalar─▒
src/Settings.h/.cpp         map_settings.ini okuma/yazma yard─▒mc─▒s─▒ (kopya; motor taraf─▒nda da var)
```

Haritaya **sadece** `PrimeMap.h` (API) ├╝zerinden eri┼ƒilir.

## ├£├º├╝nc├╝ Taraf Bile┼ƒenler

┼×irket kural─▒ gere─ƒi ├╝├º├╝nc├╝ taraf bile┼ƒenler repoda tutulmaz;
`C:\workspace\cots` alt─▒ndan okunur:

| Bile┼ƒen | S├╝r├╝m | Ne i├ºin kullan─▒l─▒yor | ─░ndirme | Kurulum klas├Âr├╝ |
|---|---|---|---|---|
| X-Plane SDK | 3.0 (XPLM301) | X-Plane eklenti aray├╝z├╝: ba┼ƒl─▒klar + `XPLM_64.lib` | https://developer.x-plane.com/sdk/ | `C:\workspace\cots\X-Plane-SDK` |

## Gereksinim: prime_map_sdk

Derlemeden ├Ânce SDK paketi gerekir. `prime_map_api` projesinden:

```bat
cmake --install <api_derleme_klasoru> --prefix <yol>\prime_map_sdk
```

Varsay─▒lan beklenti: bu klas├Âr├╝n karde┼ƒi olan `../prime_map_sdk`.
Farkl─▒ yerdeyse: `-DPRIME_MAP_SDK=<klasor>`.

## Derleme

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build_cmd -G Ninja -DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe ^
      -DCMAKE_BUILD_TYPE=Release -DMAP_DEPLOY=ON
ninja -C build_cmd
```

`MAP_DEPLOY=ON` (varsay─▒lan) derleme sonras─▒ `win.xpl`'yi
`<X-Plane>/Resources/plugins/prime-map-plugin/64/` alt─▒na kopyalar
(X-Plane a├º─▒kken dosya kilitli; ├Ânce kapat).

`/MD` k├╝t├╝phanesi kullanmak i├ºin: SDK'y─▒ `/MD` ile derleyip ayn─▒ klas├Âre kurun ve
burada `-DPRIME_MAP_DYNAMIC_CRT=ON` verin.

## Dosyalar (eklenti klas├Âr├╝nde)

| Dosya | ─░├ºerik |
|---|---|
| `map_settings.ini` | pencere, g├Âr├╝n├╝m, ├Ânbellek, katmanlar (host yazar) |
| `map_drawings.txt` | kullan─▒c─▒ ├ºizimleri |
| `terrain_colors.ini` | zemin renk ayarlar─▒ |
| `cache/` | zemin/havaliman─▒ ├Ânbelle─ƒi |
