# Prime Map API — CHANGELOG

## 1.0.0 (prime-map-plugin 0.20, 2026-11-08)

İlk sürüm. X-Plane'de henüz denenmedi.

- Tek genel başlık `PrimeMap.h`; statik kütüphane `prime_map_MT[d].lib` / `prime_map_MD[d].lib` (LZMA içinde).
- `Init` / `Shutdown` / `GetStatus` / `VersionString`.
- `Map::Open` (tek harita), `Embedded` ve `Standalone` pencere, UI ve fare anahtarları
  (fare kapalıyken tıklama alttaki pencereye geçer).
- Görünüm: merkez, zoom, zoom/pan adımları, `PanBy`, `FitBounds`, takip, `GoToAirport`, `FindAirports`,
  `ScreenToGeo` / `GeoToScreen`.
- Modlar, `MapLayer` enum'u ve panel kuralları, kullanıcı çizimleri, uçak/rota kaynağı (X-Plane ya da host),
  sorgu (`QueryAt`), ayar ve zemin ayarı dışa/içe aktarma (+ revizyon sayaçları), ön önbellek, teşhis.
- Tasarım taslağına göre eklenenler: `SetPoppedOut`, `GetSettingsRevision`, `GetDrawingsRevision`, `KindOf`,
  `TopLevelLayers`, `IsLayerDefaultVisible`, `MinZoom`/`MaxZoom`, `IsInitialized`, `Get...Source`.
  `ParentOf` `std::optional` döner. `[cache]` ayarları `InitConfig`'e, `[render]`/`[text]` `OpenConfig`'e taşındı.
