# plugin_api_test

Prime Map API'yi denemek i├ºin ayr─▒ bir X-Plane 11 eklentisi: kendi UI'─▒ +
SDK paketiyle harita g├Âmme testi (Ad─▒m 1 UI, Ad─▒m 2 API ba─ƒlama).
B├╝t├╝n API ├ºa─ƒr─▒lar─▒ `src/MapApiCalls.cpp` i├ºindedir; liste: `docs/API_LISTESI.md`.

## Derleme

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build -G Ninja -DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe -DCMAKE_BUILD_TYPE=Release
ninja -C build   ->  build/plugin_api_test/64/win.xpl
```

`win.xpl`'yi X-Plane'e kurmak i├ºin: `<X-Plane>\Resources\plugins\plugin-api-test\64\`
(X-Plane kapal─▒yken).

## ├£├º├╝nc├╝ Taraf Bile┼ƒenler

┼×irket kural─▒ gere─ƒi ├╝├º├╝nc├╝ taraf bile┼ƒenler repoda tutulmaz;
`C:\workspace\cots` alt─▒ndan okunur:

| Bile┼ƒen | S├╝r├╝m | Ne i├ºin kullan─▒l─▒yor | ─░ndirme | Kurulum klas├Âr├╝ |
|---|---|---|---|---|
| X-Plane SDK | 3.0 (XPLM301) | X-Plane eklenti aray├╝z├╝: ba┼ƒl─▒klar + `XPLM_64.lib` | https://developer.x-plane.com/sdk/ | `C:\workspace\cots\X-Plane-SDK` |

Ayr─▒ca `prime_map_sdk` paketi gerekir: `-DPRIME_MAP_SDK=<klasor>`
(varsay─▒lan: `../prime_map_sdk`).
