# Prime Map API ÔÇö ├ça─ƒr─▒ Listesi (v1.0.0)

Tek genel ba┼ƒl─▒k: `include/PrimeMap.h`. ─░stisna yok: ge├ºersiz de─ƒer ÔåÆ i┼ƒlem yap─▒lmaz,
Log'a tek sat─▒r uyar─▒, d├Ân├╝┼ƒ `false` / bo┼ƒ. T├╝m ├ºa─ƒr─▒lar X-Plane **ana thread**'inde.

T├╝m s─▒n─▒f─▒n kaynak hali: `prime_map_sdk/include/PrimeMap.h`.
Her ├ºa─ƒr─▒n─▒n sarmalay─▒c─▒s─▒: `prime_map_sdk/examples/map_api_test/src/MapApiCalls.cpp`
(bu eklentideki d├╝─ƒmeler de bu metotlar─▒ ├ºa─ƒ─▒racak).

---

## 1. Ya┼ƒam d├Âng├╝s├╝ (Map'ten ba─ƒ─▒ms─▒z, mod├╝l d├╝zeyi)

| ├ça─ƒr─▒ | Ne yapar |
|---|---|
| `PrimeMap::Init(const InitConfig&)` | K├╝t├╝phaneyi ba┼ƒlat─▒r; veri y├╝klemesini arka planda ba┼ƒlat─▒r. ─░kinci ├ºa─ƒr─▒ `false` d├Âner |
| `PrimeMap::Shutdown()` | Arka plan i┼ƒlerini durdurur. ├ûnce harita kapat─▒lm─▒┼ƒ olmal─▒ |
| `PrimeMap::IsInitialized()` | Ba┼ƒlat─▒ld─▒ m─▒ |
| `PrimeMap::GetStatus()` | `Status`: nav/havaliman─▒/hava yolu/zemin haz─▒r m─▒, ka├º zemin resmi bekliyor, alt ┼ƒerit metni |
| `PrimeMap::VersionString()` | "1.0.0" |

`InitConfig` alanlar─▒: `mCacheDirectory` (bo┼ƒ = host eklenti klas├Âr├╝/cache/), `mLogPrefix`,
`mLogCallback` (log sat─▒rlar─▒n─▒ host'a y├Ânlendirir), `mTerrainCacheMb`, `mPrefetchTiles`.

## 2. Harita penceresi ÔÇö `PrimeMap::Map`

| ├ça─ƒr─▒ | Ne yapar |
|---|---|
| `Map::Open(const OpenConfig&)` | Harita penceresini a├ºar. **Tek harita**: a├º─▒kken ikinci `Open` ÔåÆ `nullptr` |
| `~Map()` / `reset()` | Pencereyi kapat─▒r |
| `SetGeometry(Geometry)` | Yer + boyut (boxel; sol-├╝st k├Â┼ƒe + geni┼ƒlik/y├╝kseklik). Embedded stilin yer de─ƒi┼ƒtirme yolu |
| `GetGeometry()` | ÔÇö |
| `SetVisible(bool)` / `IsVisible()` | Haritay─▒ g├Âster/gizle |
| `BringToFront()` | Pencereyi en ├Âne al─▒r |
| `SetPoppedOut(bool)` / `IsPoppedOut()` | Sadece Standalone stil: X-Plane d─▒┼ƒ─▒na ├º─▒kar |
| `SetUiVisible(bool)` | Haritan─▒n kendi ├╝st ┼ƒeridi, paneller, durum sat─▒r─▒, tooltip kutusu |
| `SetMouseInput(bool)` | S├╝r├╝kleme / tekerlek / tooltip / ├ºizim t─▒klamas─▒ (UI'dan ba─ƒ─▒ms─▒z). Kapal─▒yken t─▒klama alttaki pencereye ge├ºer |

`OpenConfig`: `mGeometry`, `mStyle` (Embedded = ├ºer├ºevesiz / Standalone = ta┼ƒ─▒n─▒r pencere),
`mMode`, `mUiVisible`, `mMouseInput`, `mVisible`, `mImageCache` (FBO ├Ânbelle─ƒi), `mMaxRenderFps`, `mXPlaneFont`.

## 3. G├Âr├╝n├╝m

| ├ça─ƒr─▒ | Ne yapar |
|---|---|
| `SetCenter(lat, lon)` / `GetCenter()` | Harita merkezini koy/oku |
| `SetZoom(z)` / `GetZoom()` | Zoom 3..16.5 (d─▒┼ƒ─▒ndaysa k─▒rp─▒l─▒r) |
| `MinZoom()` / `MaxZoom()` | S─▒n─▒rlar |
| `ZoomIn()` / `ZoomOut()` | Bir kademe (varsay─▒lan 0.5 = bir tekerlek tiki) |
| `SetZoomStep(z)` | Kademe miktar─▒ |
| `PanLeft/Right/Up/Down()` | G├Âr├╝nt├╝n├╝n %25'i kadar kayd─▒r─▒r |
| `SetPanStep(frac)` | Kayd─▒rma ad─▒m─▒ (g├Âr├╝nt├╝n├╝n kesri, varsay─▒lan 0.25) |
| `PanBy(dx, dy)` | Boxel cinsinden kayd─▒rma (s├╝r├╝kleme ile ayn─▒ y├Ân) |
| `FitBounds(GeoBounds)` | Verilen enlem/boylam kutusunu s─▒─ƒd─▒r─▒r |
| `GetVisibleBounds()` | ┼×u an g├Âr├╝nen kutu |
| `GetMetersPerBoxel()` | Merkez enleminde ├Âl├ºek |
| `SetFollowAircraft(bool)` | U├ºa─ƒ─▒ takip a├º/kapa (elle kayd─▒rma takibi kapat─▒r) |
| `CenterOnAircraft()` | Merkezi u├ºa─ƒa al─▒r |
| `GoToAirport("KSEA")` | Sadece harita gider (u├ºak de─ƒi┼ƒmez) |
| `FindAirports(query, max)` | ICAO/ad aramas─▒ ÔåÆ `AirportMatch` listesi |
| `ScreenToGeo(x, y)` / `GeoToScreen(lat, lon, &x, &y)` | Ekran Ôåö co─ƒrafya d├Ân├╝┼ƒ├╝m├╝ |

## 4. Mod

| ├ça─ƒr─▒ | Ne yapar |
|---|---|
| `SetMode(MapMode)` | Scenery / Vfr / IfrLow / IfrHigh / Dark |
| `GetMode()` | ÔÇö |
| `NextMode()` | S─▒radaki moda ge├º |

## 5. Katmanlar (kurallar katman paneliyle birebir ayn─▒)

| ├ça─ƒr─▒ | Ne yapar |
|---|---|
| `SetLayerVisible(MapLayer, bool)` | Katman─▒/alt katman─▒ a├º-kapa |
| `IsLayerVisible(MapLayer)` | Kendi kutusu (├╝st ├Â─ƒe kapal─▒ysa yine ├ºizilmez) |
| `ToggleLayer(MapLayer)` | Panel kutusuna t─▒klaman─▒n ayn─▒s─▒ (grupta: hepsi a├º─▒ksa hepsini kapat─▒r) |
| `SetLayersVisible({...}, bool)` | Toplu a├º-kapa |
| `ShowOnlyLayers({...})` | Verilenler hari├º hepsini kapat─▒r |
| `ResetLayersToDefaults()` | Modun varsay─▒lanlar─▒ |
| `GetLayerState(MapLayer)` | Off / On / Mixed (grup kutusundaki [-]) |
| `ParentOf / ChildrenOf / TopLevelLayers` | Katman a─ƒac─▒ |
| `KindOf` / `LayerName` / `LayerKey` / `LayerFromKey` | T├╝r (grup/katman/alt), panel ad─▒, ayar anahtar─▒ ("runways.water") |
| `IsLayerDefaultVisible(layer, mode)` | Varsay─▒lan g├Âr├╝n├╝rl├╝k |

`MapLayer` enum'u (s─▒ra paneliyle ayn─▒): `MapGroup` (Terrain, TerrainForests, TerrainBuiltUp,
Roads + 6 alt, Grid), `AirportsGroup` (AirportSymbols, Runways + 6 alt, Surfaces + 4 alt,
Objects + 13 alt, TaxiRoutes), `NavaidsGroup` (Vor, Ndb, Ils, Fixes), `AirwaysGroup`
(Low, High), `FlightGroup` (Aircraft, FmsRoute, FlightTrack), `Drawings` (+ 4 alt).

## 6. ├çizimler (kullan─▒c─▒ ├ºizimleri)

| ├ça─ƒr─▒ | Ne yapar |
|---|---|
| `SetDrawTool(DrawTool)` | Pan / Point / Route / Region |
| `GetDrawTool()` | ÔÇö |
| `AddDrawingPoint(lat, lon)` | A├º─▒k ├ºizime nokta ekler (Pan arac─▒nda `false`) |
| `FinishDrawing()` / `UndoDrawing()` / `HasUnfinishedDrawing()` | ├çizimi bitir / son noktay─▒ al / a├º─▒k ├ºizim var m─▒ |
| `ClearDrawings()` | Hepsini siler |
| `GetShapes()` / `RemoveShape(id)` | ├çizimleri oku / tek sil |
| `ExportDrawings()` / `ImportDrawings(text)` | map_drawings.txt bi├ºimi |

## 7. U├ºak, iz, rota

| ├ça─ƒr─▒ | Ne yapar |
|---|---|
| `SetAircraftSource(DataSource)` | `XPlane` (dataref, varsay─▒lan) / `Host` |
| `SetAircraft(AircraftState)` | Host iken **her karede** konum/heading/h─▒z/irtifa |
| `GetAircraft()` | ÔÇö |
| `ClearTrack()` | U├ºu┼ƒ izini temizler |
| `SetRouteSource(DataSource)` / `SetRoute(waypoints, activeIndex)` | FMS yerine host rotas─▒ |

## 8. Sorgu

| ├ça─ƒr─▒ | Ne yapar |
|---|---|
| `QueryAt(lat, lon)` / `QueryAtScreen(x, y)` | Son ├ºizilen kareye g├Âre "burada ne var" ÔåÆ `MapObject` (katman + tooltip metni) |

## 9. Ayarlar

| ├ça─ƒr─▒ | Ne yapar |
|---|---|
| `ExportSettings()` / `ImportSettings(text)` | G├Âr├╝n├╝m + mod katmanlar─▒ + ara├ºlar metni |
| `GetTerrainSettings()` / `SetTerrainSettings(...)` | `terrain_colors.ini`'nin t├╝m anahtarlar─▒ (renkler, shading, warp, relief, water, vfr) |
| `ExportTerrainSettings()` / `ImportTerrainSettings(text)` | ini metni (yorumlar─▒yla) |
| `GetSettingsRevision()` / `GetDrawingsRevision()` | De─ƒi┼ƒince artan saya├ºlar; host bak─▒p de─ƒi┼ƒtiyse dosyaya yazar |

## 10. ├ûn ├Ânbellek ve te┼ƒhis

| ├ça─ƒr─▒ | Ne yapar |
|---|---|
| `PrecacheVisibleArea()` | G├Âr├╝nen alan─▒ ├Ânbelle─ƒe al─▒r (s─▒n─▒r─▒n %80'ini a┼ƒarsa ba┼ƒlamaz) |
| `PrecacheArea(GeoBounds)` | ─░stedi─ƒin b├Âlgeyi ├Ânbelle─ƒe al─▒r (u├ºu┼ƒ ├Âncesi y├╝kleme) |
| `CancelPrecache()` | ─░ptal |
| `SetDiagnostics(bool)` | FPS / draw ms te┼ƒhisi (`Status::mPerformanceText`) |

---

## Bu eklentinin d├╝─ƒmeleri hangi ├ºa─ƒr─▒ya ba─ƒlanacak (Ad─▒m 3)

| D├╝─ƒme | ├ça─ƒr─▒ |
|---|---|
| `^` (yukar─▒) | `mMap->PanUp()` |
| `v` (a┼ƒa─ƒ─▒) | `mMap->PanDown()` |
| `<` (sol) | `mMap->PanLeft()` |
| `>` (sa─ƒ) | `mMap->PanRight()` |
| `+` | `mMap->ZoomIn()` |
| `-` | `mMap->ZoomOut()` |

## Ad─▒m-ad─▒m ba─ƒlama s─▒ras─▒ (Ad─▒m 2-3)

1. `PrimeMap::Init({})` ÔÇö eklenti a├º─▒l─▒┼ƒ─▒nda (bir kez).
2. `PrimeMap::Map::Open({...})` ÔÇö haritay─▒ harita alan─▒na denk geometriyle a├º;
   `mStyle = Embedded`, `mUiVisible = false`, `mMouseInput = false`.
3. `mMap->SetCenter(47.45, -122.30); mMap->SetZoom(11);` ÔÇö ba┼ƒlang─▒├º g├Âr├╝n├╝m├╝ (KSEA).
4. Pencere ta┼ƒ─▒n─▒r/boyutlan─▒rsa draw'da harita alan─▒ dikd├Ârtgenini yeniden hesaplay─▒p
   `mMap->SetGeometry(...)` ├ºa─ƒ─▒r.
5. `Press()` i├ºinde ilgili `mMap->Pan...()/ZoomIn()/ZoomOut()` ├ºa─ƒr─▒s─▒.
6. `XPluginDisable`'da s─▒rayla: `mMap.reset()` ÔåÆ `PrimeMap::Shutdown()`.
