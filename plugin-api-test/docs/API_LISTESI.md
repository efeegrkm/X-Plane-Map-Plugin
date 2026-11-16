# Prime XMap API — Çağrı Listesi (v0.26.0)

Tam kullanım kılavuzu ve referans: `prime_map_api/docs/PrimeMap_API.pdf`. Bu dosya kısa çağrı listesidir.

Kaynak: `prime_map_api/src/api/{XMapManagerFactory,XMapManager,XMap,XMapTypes}.h` (toplama başlık
`#include <prime_map_api/PrimeMap.h>`). Namespace `prime`. İstisna yok: geçersiz değer → işlem yapılmaz,
Log'a tek satır uyarı, dönüş `false` / boş. Bütün çağrılar X-Plane **ana thread**'inden.

Bu eklentide her çağrı `src/XMapApiCalls.cpp`'de tek satırdır (`SDK_BOUND` yorumlu).

## 1. Yaşam döngüsü

```cpp
prime::XMapManager* tManager = prime::XMapManagerFactory::createXMapManager();  // her cagri YENI yonetici
tManager->setLogPrefix("[host]");               // initialize oncesi ayarlar (hepsi istege bagli)
tManager->initialize();                         // veri yuklemesi arka planda baslar
prime::XMap* tMap = tManager->addXMap(tConfig); // nesne; pencere yok
tMap->initialize();                             // pencere acilir, ayar dosyalari okunur
...
tManager->removeXMap(tMap);                     // ayarlari yazar, nesneyi yok eder
tManager->shutdown();
prime::XMapManagerFactory::destroyXMapManager(tManager);
```

| Çağrı | Ne yapar |
|---|---|
| `XMapManagerFactory::createXMapManager()` | Yeni yönetici (ve ayrı önbellek bütçeleri). Singleton yoktur |
| `XMapManagerFactory::destroyXMapManager(m)` | Yöneticiyi ve kalan haritaları bırakır |
| `prime::versionString()` | `"0.26.0"` |

## 2. XMapManager

| Çağrı | Ne yapar |
|---|---|
| `setDataDirectory(path)` | Veri klasörü (boş = `%LOCALAPPDATA%/prime/prime-xmap/`). `initialize` öncesi |
| `setCacheDirectory(path)` | Önbellek klasörü (boş = `<veri>/cache/`). `initialize` öncesi |
| `setLogPrefix(prefix)`, `setLogCallback(fn)` | Log.txt satır öneki / satırları host'a da ilet |
| `setTerrainCacheMb(mb)`, `setPrefetchTiles(n)` | `prime_map.json` henüz yoksa ilk değerler (verilmezse 1024 MB / 20 karo) |
| `initialize()`, `shutdown()`, `isInitialized()` | Ortak veriyi başlat / durdur |
| `getStatus()` | `Status`: nav / havalimanı / hava yolu / zemin hazır mı, bekleyen zemin resmi, ön önbellek, açık harita, sınırlar |
| `addXMap(XMapConfig = {})` | Yeni harita nesnesi (açılmaz) |
| `removeXMap(xmap)` | Ayarlarını yazar, nesneyi yok eder |
| `findXMap(id)`, `findXMap(name)`, `getXMaps()`, `getOpenXMapCount()` | Haritaları bul / say |
| `getSettings()`, `setSettings(XMapManagerSettings)` | Merkezi ayarlar (`prime_map.json`): ortak önbellek / doku bütçesi, otomatik kayıt |
| `getDataDirectory()`, `getSettingsFilePath()`, `getXMapsDirectory()`, `getCacheDirectory()` | Yollar; **`initialize` öncesi de** doğru döner |
| `parentOf`, `childrenOf`, `topLevelLayers`, `kindOf`, `layerName`, `layerKey`, `layerFromKey`, `isLayerDefaultVisible(layer, mode)` | Katman ağacı ve kuralları |

`XMapConfig`: `mName`, `mPersistence`, `mGeometry`, `mStyle` (varsayılan `Texture`), `mMode`, `mUiVisible`, `mMouseInput`, `mVisible`,
`mImageCache`, `mMaxRenderFps`, `mXPlaneFont` (hepsi sonradan da ayarlanabilir).

## 3. XMap

| Grup | Çağrılar |
|---|---|
| Yaşam | `initialize`, `close`, `isInitialized`, `isDataLoaded`, `onRender` |
| Kimlik / dosyalar | `getId`, `setName` / `getName` (ayar klasörü `maps/<ad>/`; boş = `map_<id>`), `getSettingsDirectory`, `setPersistence` / `getPersistence` (`Off`, `SaveOnly`, `LoadAndSave`), `saveSettings` |
| Pencere | `setStyle` / `getStyle` (`Texture`; LEGACY: `Embedded`, `Standalone`; sadece kapalıyken), `setTitle`, `setGeometry` / `getGeometry`, `setVisible` / `isVisible`, `bringToFront`, `setPoppedOut` / `isPoppedOut`, `setIsUiVisible` / `isUiVisible`, `setIsMouseInputEnabled` / `isMouseInputEnabled` |
| Görünüm | `setCenter` / `getCenter`, `setZoom` / `getZoom`, `minZoom`, `maxZoom`, `hardMaxZoom`, `setZoomLimits`, `getMinZoomLimit`, `getMaxZoomLimit`, `zoomIn`, `zoomOut`, `setZoomStep` / `getZoomStep`, `panLeft/Right/Up/Down`, `setPanStep` / `getPanStep`, `panBy`, `fitBounds`, `getVisibleBounds`, `getMetersPerBoxel`, `setFollowAircraft` / `isFollowingAircraft`, `centerOnAircraft`, `goToAirport`, `findAirports`, `screenToGeo`, `geoToScreen` |
| Mod | `setMode` / `getMode` (`Scenery`, `Vfr`, `IfrLow`, `IfrHigh`, `Dark`), `nextMode` |
| Katmanlar | `setIsLayerVisible`, `isLayerVisible`, `toggleLayer`, `setLayersVisible`, `showOnlyLayers`, `resetLayersToDefaults`, `getLayerState` (`Off`/`On`/`Mixed`), `setLayerZoomRange` / `getLayerZoomRange` / `clearLayerZoomRange` / `clearAllLayerZoomRanges` |
| Katman rengi | `setColor(layer, Rgb)`, `getColor` (yoksa `mR < 0`), `clearColor`, `clearAllColors` — dosyada `layer_colors` |
| Çizimler | `setDrawTool` / `getDrawTool` (`Pan`, `Point`, `Route`, `Region`), `addDrawingPoint`, `finishDrawing`, `undoDrawing`, `clearDrawings`, `hasUnfinishedDrawing`, `getShapes`, `removeShape`, `exportDrawings`, `importDrawings` |
| Uçak / iz / rota | `setAircraftSource` / `getAircraftSource` (`XPlane`, `Host`), `setAircraft` / `getAircraft`, `clearTrack`, `setRouteSource` / `getRouteSource`, `setRoute` |
| Sorgu | `queryAt(lat, lon)`, `queryAtScreen(x, y)` |
| Çizim seçenekleri | `setImageCache` / `isImageCacheEnabled`, `setMaxRenderFps` / `getMaxRenderFps`, `setXPlaneFont` / `isXPlaneFont` |
| Ayarlar | `exportSettings` / `importSettings` (JSON), `getTerrainSettings` / `setTerrainSettings`, `exportTerrainSettings` / `importTerrainSettings`, `getSettingsRevision`, `getDrawingsRevision` |
| Durum | `getStatusText`, `getTerrainPendingImages` |
| Doku (`WindowStyle::Texture`) | `setTextureSize(w, h, pixelsPerUnit = 0)`, `getTextureWidth/Height`, `renderTexture`, `getTextureId`, `injectMouse(x, y, MouseAction, MouseButton)`, `injectMouseMove`, `injectWheel`, `injectKey`, `wantsKeyboard` — ayrıntı aşağıda |
| Zemin ön önbelleği / teşhis | `precacheVisibleArea`, `precacheArea`, `cancelPrecache`, `setDiagnostics` / `isDiagnosticsOn` |

Katman anahtarları (`XMapLayer`): `XMapGroup`, `Terrain` (+`TerrainForests`, `TerrainBuiltUp`), `Roads` (+6 alt),
`Grid`, `AirportsGroup`, `AirportSymbols`, `Runways` (+6), `Surfaces` (+4), `Objects` (+12), `TaxiRoutes`,
`NavaidsGroup` (`Vor`, `Ndb`, `Ils`, `Fixes`), `AirwaysGroup` (`AirwaysLow`, `AirwaysHigh`), `FlightGroup`
(`Aircraft`, `FmsRoute`, `FlightTrack`), `Drawings` (+4). Dosya anahtarları: `XMapManager::layerKey`.

## 4. Doku modu (`WindowStyle::Texture`) — hedef yol

Harita XPLM penceresi açmaz; bir openGL dokusuna çizilir ve host onu istediği yere basar (kendi penceresi,
panel, kokpit ekranı). Yer eşleme, pencere sırası ve tıklama çakışması sorunu yoktur.

```cpp
prime::XMapConfig tConfig;
tConfig.mStyle = prime::WindowStyle::Texture;
tConfig.mUiVisible = true;   // haritanin kendi seritleri ve panelleri dokunun icinde
tConfig.mMouseInput = true;
prime::XMap* tMap = tManager->addXMap(tConfig);
tMap->setTextureSize(600, 400);  // boxel; doku pikseli UI olcegine gore otomatik (0)
tMap->initialize();

// host'un XPLM cizim callback'i icinde:
if (tMap->renderTexture())   // gerekiyorsa yeniden cizer (max_fps, degisiklik, girdi)
{
    XPLMSetGraphicsState(0, 1, 0, 0, 0, 0, 0);
    XPLMBindTexture2d(static_cast<int>(tMap->getTextureId()), 0);
    // dortgen: (0,0) sol-alt ... (1,1) sag-ust (v = 0 alt kenar)
}
// girdi: koordinat dokuya gore, birim boxel, orijin SOL-ALT, y yukari
tMap->injectMouse(x - left, y - bottom, prime::MouseAction::Down);   // Drag, Up; MouseButton::Right
tMap->injectMouseMove(x - left, y - bottom);   // imlec ustundeyken her karede (cursor callback)
tMap->injectWheel(x - left, y - bottom, clicks);
if (tMap->wantsKeyboard()) { /* XPLMTakeKeyboardFocus(hostPenceresi); tuslar -> injectKey */ }
```

Kurallar: `renderTexture` sadece host'un çizim callback'i içinde (GL bağlamı); `false` dönerse dokuyu
basmayın. Kütüphane GL program / VAO / tampon bağlamalarını geri koyar ve dokuları `XPLMBindTexture2d` ile
bağlar. Örnek: `plugin-api-test` (`src/XMapUiTestMain.cpp`: `drawMapTexture`, `handleClick` ... `syncKeyboardFocus`).

## 5. Dosyalar (veri klasöründe)

| Dosya | Ne |
|---|---|
| `prime_map.json` | Merkezi ayarlar |
| `maps/<ad>/map_settings.json`, `map_drawings.txt`, `terrain_colors.json` | Harita başına (biçim yalnızca JSON) |
| `cache/terrain/`, `cache/airports/` | Ortak önbellek |
