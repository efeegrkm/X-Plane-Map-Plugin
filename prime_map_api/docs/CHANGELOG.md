# Prime Map API ÔÇö CHANGELOG

## 1.0.0 (prime-map-plugin 0.20, 2026-11-08)

─░lk s├╝r├╝m. ├ûrnek/test eklentisi: `examples/map_api_test`.

- Tek genel ba┼ƒl─▒k `PrimeMap.h`; statik k├╝t├╝phane `prime_map_MT[d].lib` / `prime_map_MD[d].lib` (LZMA i├ºinde).
- `Init` / `Shutdown` / `GetStatus` / `VersionString`.
- `Map::Open` (tek harita), `Embedded` ve `Standalone` pencere, UI ve fare anahtarlar─▒
  (fare kapal─▒yken t─▒klama alttaki pencereye ge├ºer).
- G├Âr├╝n├╝m: merkez, zoom, zoom/pan ad─▒mlar─▒, `PanBy`, `FitBounds`, takip, `GoToAirport`, `FindAirports`,
  `ScreenToGeo` / `GeoToScreen`.
- Modlar, `MapLayer` enum'u ve panel kurallar─▒, kullan─▒c─▒ ├ºizimleri, u├ºak/rota kayna─ƒ─▒ (X-Plane ya da host),
  sorgu (`QueryAt`), ayar ve zemin ayar─▒ d─▒┼ƒa/i├ºe aktarma (+ revizyon saya├ºlar─▒), ├Ân ├Ânbellek, te┼ƒhis.
- Tasar─▒m tasla─ƒ─▒na g├Âre eklenenler: `SetPoppedOut`, `GetSettingsRevision`, `GetDrawingsRevision`, `KindOf`,
  `TopLevelLayers`, `IsLayerDefaultVisible`, `MinZoom`/`MaxZoom`, `IsInitialized`, `Get...Source`.
  `ParentOf` `std::optional` d├Âner. `[cache]` ayarlar─▒ `InitConfig`'e, `[render]`/`[text]` `OpenConfig`'e ta┼ƒ─▒nd─▒.