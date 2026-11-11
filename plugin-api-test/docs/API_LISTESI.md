# Prime Map API — Çağrı Listesi (v0.22.0)

Tek genel başlık: `PrimeMap.h`. İstisna yok: geçersiz değer → işlem yapılmaz,
Log'a tek satır uyarı, dönüş `false` / boş. Tüm çağrılar X-Plane **ana thread**'inde.

Namespace: `prime`. Kütüphane nesnesi: `prime::MapLibrary::Get()` (süreçte tek örnek;
SHARED DLL olduğu için aynı X-Plane sürecindeki tüm host eklentileri paylaşır).

---

## 1. Yaşam döngüsü (Map'ten bağımsız, modül düzeyi)

| Çağrı | Ne yapar |
|---|---|
| `prime::MapLibrary::Get().Init(const InitConfig&)` | Kütüphane nesnesini başlatır; veri yüklemesini arka planda başlatır. Zaten çalışıyorsa (başka host) ek iş yapmaz, `true` döner (referans sayaçlı) |
| `prime::MapLibrary::Get().Shutdown()` | Referans sayacını düşürür; son host çıkarken arka plan işleri durur, açık haritalar kapatılır |
| `prime::MapLibrary::Get().IsInitialized()` | Başlatıldı mı |
| `prime::MapLibrary::Get().GetStatus()` | `Status`: nav/havalimanı/hava yolu/zemin hazır mı, kaç zemin resmi bekliyor, alt şerit metni |
| `prime::VersionString()` | "0.22.0" |

`InitConfig` alanları: `mCacheDirectory` (boş = host eklenti klasörü/cache/), `mLogPrefix`,
`mLogCallback` (log satırlarını host'a yönlendirir), `mTerrainCacheMb`, `mPrefetchTiles`.

## 2. Harita penceresi — `prime::Map`

| Çağrı | Ne yapar |
|---|---|
| `MapLibrary::Get().CreateMap(const OpenConfig&)` | Yeni harita penceresi açar. **Çoklu harita**: aynı anda birden çok `Map` nesnesi yaşayabilir |
| `Map::Open(const OpenConfig&)` | `CreateMap` ile aynı (geriye uyumluluk) |
| `~Map()` / `reset()` | Pencereyi kapatır |
| `SetGeometry(Geometry)` | Yer + boyut (boxel; sol-üst köşe + genişlik/yükseklik). Embedded stilin yer değiştirme yolu |
| `GetGeometry()` | — |
| `SetVisible(bool)` / `IsVisible()` | Haritayı göster/gizle |
| `BringToFront()` | Pencereyi en öne alır |
| `SetPoppedOut(bool)` / `IsPoppedOut()` | Sadece Standalone stil: X-Plane dışına çıkar |
| `SetUiVisible(bool)` | Haritanın kendi üst şeridi, paneller, durum satırı, tooltip kutusu |
| `SetMouseInput(bool)` | Sürükleme / tekerlek / tooltip / çizim tıklaması (UI'dan bağımsız). Kapalıyken tıklama alttaki pencereye geçer |

`OpenConfig`: `mGeometry`, `mStyle` (Embedded = çerçevesiz / Standalone = taşınır pencere),
`mMode`, `mUiVisible`, `mMouseInput`, `mVisible`, `mImageCache` (FBO önbelleği), `mMaxRenderFps`, `mXPlaneFont`.

## 3. Görünüm

| Çağrı | Ne yapar |
|---|---|
| `SetCenter(lat, lon)` / `GetCenter()` | Harita merkezini koy/oku |
| `SetZoom(z)` / `GetZoom()` | Zoom 3..16.5 (dışındaysa kırpılır) |
| `MinZoom()` / `MaxZoom()` | Sınırlar |
| `ZoomIn()` / `ZoomOut()` | Bir kademe (varsayılan 0.5 = bir tekerlek tiki) |
| `SetZoomStep(z)` | Kademe miktarı |
| `PanLeft/Right/Up/Down()` | Görüntünün %25'i kadar kaydırır |
| `SetPanStep(frac)` | Kaydırma adımı (görüntünün kesri, varsayılan 0.25) |
| `PanBy(dx, dy)` | Boxel cinsinden kaydırma (sürükleme ile aynı yön) |
| `FitBounds(GeoBounds)` | Verilen enlem/boylam kutusunu sığdırır |
| `GetVisibleBounds()` | Şu an görünen kutu |
| `GetMetersPerBoxel()` | Merkez enleminde ölçek |
| `SetFollowAircraft(bool)` | Uçağı takip aç/kapa (elle kaydırma takibi kapatır) |
| `CenterOnAircraft()` | Merkezi uçağa alır |
| `GoToAirport("KSEA")` | Sadece harita gider (uçak değişmez) |
| `FindAirports(query, max)` | ICAO/ad araması → `AirportMatch` listesi |
| `ScreenToGeo(x, y)` / `GeoToScreen(lat, lon, &x, &y)` | Ekran ↔ coğrafya dönüşümü |

## 4. Mod

| Çağrı | Ne yapar |
|---|---|
| `SetMode(MapMode)` | Scenery / Vfr / IfrLow / IfrHigh / Dark |
| `GetMode()` | — |
| `NextMode()` | Sıradaki moda geç |

## 5. Katmanlar (kurallar katman paneliyle birebir aynı)

| Çağrı | Ne yapar |
|---|---|
| `SetLayerVisible(MapLayer, bool)` | Katmanı/alt katmanı aç-kapa |
| `IsLayerVisible(MapLayer)` | Kendi kutusu (üst öğe kapalıysa yine çizilmez) |
| `ToggleLayer(MapLayer)` | Panel kutusuna tıklamanın aynısı (grupta: hepsi açıksa hepsini kapatır) |
| `SetLayersVisible({...}, bool)` | Toplu aç-kapa |
| `ShowOnlyLayers({...})` | Verilenler hariç hepsini kapatır |
| `ResetLayersToDefaults()` | Modun varsayılanları |
| `GetLayerState(MapLayer)` | Off / On / Mixed (grup kutusundaki [-]) |
| `ParentOf / ChildrenOf / TopLevelLayers` | Katman ağacı |
| `KindOf` / `LayerName` / `LayerKey` / `LayerFromKey` | Tür (grup/katman/alt), panel adı, ayar anahtarı ("runways.water") |
| `IsLayerDefaultVisible(layer, mode)` | Varsayılan görünürlük |

`MapLayer` enum'u (sıra paneliyle aynı): `MapGroup` (Terrain, TerrainForests, TerrainBuiltUp,
Roads + 6 alt, Grid), `AirportsGroup` (AirportSymbols, Runways + 6 alt, Surfaces + 4 alt,
Objects + 13 alt, TaxiRoutes), `NavaidsGroup` (Vor, Ndb, Ils, Fixes), `AirwaysGroup`
(Low, High), `FlightGroup` (Aircraft, FmsRoute, FlightTrack), `Drawings` (+ 4 alt).

## 6. Çizimler (kullanıcı çizimleri)

| Çağrı | Ne yapar |
|---|---|
| `SetDrawTool(DrawTool)` | Pan / Point / Route / Region |
| `GetDrawTool()` | — |
| `AddDrawingPoint(lat, lon)` | Açık çizime nokta ekler (Pan aracında `false`) |
| `FinishDrawing()` / `UndoDrawing()` / `HasUnfinishedDrawing()` | Çizimi bitir / son noktayı al / açık çizim var mı |
| `ClearDrawings()` | Hepsini siler |
| `GetShapes()` / `RemoveShape(id)` | Çizimleri oku / tek sil |
| `ExportDrawings()` / `ImportDrawings(text)` | map_drawings.txt biçimi |

## 7. Uçak, iz, rota

| Çağrı | Ne yapar |
|---|---|
| `SetAircraftSource(DataSource)` | `XPlane` (dataref, varsayılan) / `Host` |
| `SetAircraft(AircraftState)` | Host iken **her karede** konum/heading/hız/irtifa |
| `GetAircraft()` | — |
| `ClearTrack()` | Uçuş izini temizler |
| `SetRouteSource(DataSource)` / `SetRoute(waypoints, activeIndex)` | FMS yerine host rotası |

## 8. Sorgu

| Çağrı | Ne yapar |
|---|---|
| `QueryAt(lat, lon)` / `QueryAtScreen(x, y)` | Son çizilen kareye göre "burada ne var" → `MapObject` (katman + tooltip metni) |

## 9. Ayarlar

| Çağrı | Ne yapar |
|---|---|
| `ExportSettings()` / `ImportSettings(text)` | Görünüm + mod katmanları + araçlar metni |
| `GetTerrainSettings()` / `SetTerrainSettings(...)` | `terrain_colors.ini`'nin tüm anahtarları (renkler, shading, warp, relief, water, vfr) |
| `ExportTerrainSettings()` / `ImportTerrainSettings(text)` | ini metni (yorumlarıyla) |
| `GetSettingsRevision()` / `GetDrawingsRevision()` | Değişince artan sayaçlar; host bakıp değiştiyse dosyaya yazar |

## 10. Ön önbellek ve teşhis

| Çağrı | Ne yapar |
|---|---|
| `PrecacheVisibleArea()` | Görünen alanı önbelleğe alır (sınırın %80'ini aşarsa başlamaz) |
| `PrecacheArea(GeoBounds)` | İstediğin bölgeyi önbelleğe alır (uçuş öncesi yükleme) |
| `CancelPrecache()` | İptal |
| `SetDiagnostics(bool)` | FPS / draw ms teşhisi (`Status::mPerformanceText`) |

---

## Bu eklentinin düğmeleri hangi çağrıya bağlanacak (Adım 3)

| Düğme | Çağrı |
|---|---|
| `^` (yukarı) | `mMap->PanUp()` |
| `v` (aşağı) | `mMap->PanDown()` |
| `<` (sol) | `mMap->PanLeft()` |
| `>` (sağ) | `mMap->PanRight()` |
| `+` | `mMap->ZoomIn()` |
| `-` | `mMap->ZoomOut()` |

## Adım-adım bağlama sırası (Adım 2-3)

1. `prime::MapLibrary::Get().Init({})` — eklenti açılışında (bir kez).
2. `prime::Map::Open({...})` — haritayı harita alanına denk geometriyle aç;
   `mStyle = Embedded`, `mUiVisible = false`, `mMouseInput = false`.
3. `mMap->SetCenter(47.45, -122.30); mMap->SetZoom(11);` — başlangıç görünümü (KSEA).
4. Pencere taşınır/boyutlanırsa draw'da harita alanı dikdörtgenini yeniden hesaplayıp
   `mMap->SetGeometry(...)` çağır.
5. `Press()` içinde ilgili `mMap->Pan...()/ZoomIn()/ZoomOut()` çağrısı.
6. `XPluginDisable`'da sırayla: `mMap.reset()` → `prime::MapLibrary::Get().Shutdown()`.
