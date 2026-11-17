# Prime XMap API — Sıfırdan Host Eklentisi Rehberi (yerel çalışma notu)

> Bu dosya kişisel çalışma notudur, repoya pushlanmaz (.gitignore'da).
> Amaç: prime-map-api kütüphanesini 0.26.0 sürümüyle sıfırdan bir X-Plane eklentisine
> harita olarak gömmenin tüm adımlarını tek yerde toplamak. Referans örnek:
> `examples/texture_host/`.

---

## 1. Ön koşullar

- X-Plane 11 kurulu
- Visual Studio 2022 (C++20), CMake >= 3.16, Ninja/VS generator
- X-Plane SDK (`cots/X-Plane-SDK`) — lzma, earcut, stb kütüphanenin içindedir, ayrıca gerekmez
- Kütüphane: Bitbucket `prime-map-api` deposu (clone edilmiş `prime_map_api` klasörü)

## 2. Klasör düzeni

```
<calisma klasoru>/
  prime_map_api/        kutuphane deposu (klasor adi include onekidir: degistirmeyin)
  my_host/              kendi eklentin
    CMakeLists.txt
    src/MyHost.cpp
```

Klonlama:

```
git clone https://gobitbucket.havelsan.com.tr/scm/eps/prime-map-api.git
```

## 3. CMake düzeni (tam liste)

`examples/texture_host/CMakeLists.txt` kopya-yapıştır tabandır. Kritik maddeler:

1. **Aynı CRT zorunlu**: kütüphane `prime_map.dll` paylaşılır; host aynı MSVC + `/MD` ile derlenmeli.
2. Kütüphane `add_subdirectory` ile aynı oturumda derlenir (hedef adı: `prime-map-api`).
3. Çıktı `win.xpl` adında olmalı, yanına `prime_map.dll` kopyalanmalı.
4. `/W4 /wd4251 /utf-8 /permissive-` — C4251 pragma yerine derleyici bayrağı.

```cmake
cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0091 NEW)                      # MSVC runtime secimi
project(my_host VERSION 1.0 LANGUAGES CXX)

set(PRIME_MAP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../prime_map_api" CACHE PATH "prime_map_api klasoru")
set(COTS_DIR "C:/workspace/cots" CACHE PATH "X-Plane-SDK burada")
set(XPSDK "${COTS_DIR}/X-Plane-SDK")
set(XPLANE_DIR "C:/X-Plane 11")
option(MAP_DEPLOY "Derlemeden sonra X-Plane'e kopyala" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")   # /MD sart

add_subdirectory(${PRIME_MAP_DIR} prime_map_build)   # hedef: prime-map-api

add_library(${PROJECT_NAME} SHARED src/MyHost.cpp)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${XPSDK}/CHeaders/XPLM)
target_compile_definitions(${PROJECT_NAME} PRIVATE XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1 IBM=1 APL=0 LIN=0
                           _CRT_SECURE_NO_WARNINGS)
target_compile_options(${PROJECT_NAME} PRIVATE /W4 /wd4251 /utf-8 /permissive-)
target_link_libraries(${PROJECT_NAME} PRIVATE prime-map-api ${XPSDK}/Libraries/Win/XPLM_64.lib opengl32)

set(OUT_DIR ${CMAKE_BINARY_DIR}/my-host)
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" OUTPUT_NAME win SUFFIX ".xpl"
    RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR}/64)

# prime_map.dll her derlemede win.xpl'nin yanina
add_custom_target(my_host_dll ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:prime-map-api> "${OUT_DIR}/64/"
    DEPENDS prime-map-api ${PROJECT_NAME})

if(MAP_DEPLOY AND EXISTS "${XPLANE_DIR}/Resources/plugins")
    set(DEPLOY_DIR "${XPLANE_DIR}/Resources/plugins/my-host")
    add_custom_command(TARGET my_host_dll POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/64"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${PROJECT_NAME}> "${DEPLOY_DIR}/64/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:prime-map-api> "${DEPLOY_DIR}/64/")
endif()
```

Kurulum: X-Plane kapalıyken `win.xpl` + `prime_map.dll` → `Resources/plugins/my-host/64/`.

## 4. Yaşam döngüsü

Resmi şablon (`PrimeMap.h` başı):

```
createXMapManager() -> setXxx + initialize() -> addXMap() + setXxx -> xmap->initialize()
```

### 4.1 Yönetici (süreç başına bir tane)

```cpp
mManager = prime::XMapManagerFactory::createXMapManager();   // kurucu ozel; fabrika zorunlu
mManager->setLogPrefix("[my-host]");                         // Log.txt satir oneki
mManager->setDataDirectory(findPluginDirectory());           // ayarlar + onbellek klasoru
if (!mManager->initialize()) { /* Log.txt'ye bak */ }
```

- `setDataDirectory` verilmezse varsayilan `%LOCALAPPDATA%/prime/prime-xmap` kullanilir.
  **Kendi klasorunu ver**: resmi Prime XMap eklentisiyle ayar/onbellek paylasilmasin.
- `initialize()` ortak veriyi (nav, havalimani, airway, zemin iscisi) arka planda yuklemeye baslar;
  cagri hemen doner. Durum: `mManager->getStatus()`.

### 4.2 Harita ekle ve ac

```cpp
prime::XMapConfig tConfig;
tConfig.mName       = "main";                        // ayarlar <veri>/maps/main/
tConfig.mStyle      = prime::WindowStyle::Texture;   // ONERILEN yol (Embedded/Standalone LEGACY)
tConfig.mMode       = prime::XMapMode::Vfr;          // Scenery / Vfr / IfrLow / IfrHigh / Dark
tConfig.mUiVisible  = true;                          // haritanin kendi seritleri/panelleri dokunun icinde
tConfig.mMouseInput = true;                          // fare/tekerlek inject* ile gelir
tConfig.mPersistence = prime::Persistence::LoadAndSave;  // varsayilan: dosya okunur+yazilir

mXMap = mManager->addXMap(tConfig);                  // nesne olusur, harita henuz acilmaz
mXMap->setTextureSize(mMapWidth, mMapHeight);        // boxel; doku pikseli UI olceginden otomatik
mXMap->initialize();                                 // ayar dosyasi okunur, ic durum kurulur
customizeMap();                                      // initialize'dan SONRA: her acilista kod gecerli
```

Sahiplik: `mManager` host'a aittir (`destroyXMapManager` ile birakilir), `mXMap` manager'a aittir
(`removeXMap` ile birakilir).

### 4.3 Kapanis sirasi (kritik)

```cpp
// XPluginDisable:
mManager->removeXMap(mXMap);        // 1) haritalar (ayarlari yazilir)
mManager->shutdown();               // 2) kutuphane
prime::XMapManagerFactory::destroyXMapManager(mManager);
XPLMDestroyWindow(mWindow);         // 3) en son pencere
```

## 5. Texture'a cizim

Texture modunda haritanin XPLM penceresi YOKTUR; kutuphane her seyi bir OpenGL dokusuna cizer.
Host cizim callback'inde (her kare):

```cpp
// 1) Alan guncelle (pencere tasindi/boyutlandi: AYNI karede gelir)
updateMapRect();                                    // XPLMGetWindowGeometry + kenar boslugu
mXMap->setTextureSize(mMapWidth, mMapHeight);       // boxel

// 2) Kutuphane gerekiyorsa dokuyu yeniden cizer
if (!mXMap->renderTexture() || mXMap->getTextureId() == 0)
    return;                                         // false: basacak doku yok

// 3) Dokuyu dortgene bas
XPLMSetGraphicsState(0, 1, 0, 0, 0, 0, 0);          // 1 doku birimi, blend yok (harita opak)
XPLMBindTexture2d((int)mXMap->getTextureId(), 0);
glColor4f(1, 1, 1, 1);
glBegin(GL_QUADS);                                  // v = 0 ALT kenar; sol-alt, sag-alt, sag-ust, sol-ust
glTexCoord2f(0.0f, 0.0f); glVertex2i(mMapLeft, mMapBottom);
glTexCoord2f(1.0f, 0.0f); glVertex2i(mMapLeft + mMapWidth, mMapBottom);
glTexCoord2f(1.0f, 1.0f); glVertex2i(mMapLeft + mMapWidth, mMapBottom + mMapHeight);
glTexCoord2f(0.0f, 1.0f); glVertex2i(mMapLeft, mMapBottom + mMapHeight);
glEnd();
```

- Doku koordinati: **boxel, orijin SOL-ALT, y yukari** (X-Plane global ekran birimi ile ayni).
- Yeniden cizim kutuphanede otomatiktir: ilk kare, boyut, sahne (mod/katman/cizim), gorunum degisimi,
  girdi; aksi halde en fazla `mMaxRenderFps` (varsayilan 15) ile yenilenir.
- `setTextureSize` ayni boyutta is yapmaz; gecici dosya/alan degisimi hic cagrilmaz.

## 6. Girdi (inject*)

Fare (doku koordinati: `tX = pX - mMapLeft`, `tY = pY - mMapBottom`):

```cpp
// MouseDown'da basmanin haritada baslayip baslamadigini belirle; basladiysa
// surukleme/birakma (alan disina ciksa da) haritaya gider:
prime::MouseAction a = prime::MouseAction::Down;    // Drag / Up'a cevir
mXMap->injectMouse(tX, tY, a, prime::MouseButton::Left);
mXMap->injectMouseMove(tX, tY);                     // cursor callback'ten her karede (tooltip/vurgu)
mXMap->injectWheel(tX, tY, pClicks);
```

Klavye (arama paneli vb.):

```cpp
if (mXMap->wantsKeyboard())                         // istediginde odaagi AL, istemeyince BIRAK
    XPLMTakeKeyboardFocus(mWindow);
// key callback:
mXMap->injectKey(pKey, (int)pFlags, pVirtualKey);
```

Kural: `wantsKeyboard()` true iken odagi tut; istemezse birak — yoksa kullanici X-Plane'e yazamaz.

## 7. Ayarlar

### 7.1 Harita basina (XMapConfig / XMap uzerinden)

| Alan/Metot | Anlami |
|---|---|
| `mName` | ayar klasoru `maps/<ad>/` (bos = `map_<id>`) |
| `mPersistence` | `Off` / `SaveOnly` / `LoadAndSave` (varsayilan: oku+yaz) |
| `mStyle` | `Texture` (onerilen) / `Embedded` / `Standalone` (LEGACY) |
| `mMode` | `Scenery / Vfr / IfrLow / IfrHigh / Dark` |
| `mUiVisible`, `mMouseInput` | kendi UI ve fare durumu |
| `mImageCache`, `mMaxRenderFps`, `mXPlaneFont` | performans/yazi secenekleri |

### 7.2 Merkezi (prime_map.json — tum haritalarin ortak onbellek butcesi)

```cpp
prime::XMapManagerSettings s = mManager->getSettings();
s.mTerrainCacheMb = 1024;                  // disk onbellegi
s.mCacheGrowthPercentPerXMap = 25;         // acik harita basina buyume
s.mTextureMemoryMb = 512;                  // bellek ici listeler
s.mAutoSaveSeconds = 2.0;
mManager->setSettings(s);
```

Limit = taban x (1 + yuzde x (acik harita - 1) / 100); tavan asilmaz. Coklu haritada haritalar
birbirinin karolarini itmesin diye bu buyume vardir.

### 7.3 Onbellek kapsami (kim kimi paylasir?)

- Paylasim birimi **yoneticidir**, surum degil. AYNI yoneticinin haritalari (orn. map1 + map2)
  ayni TerrainStore / ayni isci parcasi / ayni bellek karo havuzunu kullanir: biri bir alani
  yukleyince digerinde ANINDA hazirdir.
- Farkli yonetici (baska host eklentisi, ayri `createXMapManager`) ayri dunyadir: kendi isci
  parcasi, kendi bellek havuzu. Bellek paylasimi yoktur; ama disk klasorleri ayni verilirse karo
  diski paylasilir (guvenli: icerik deterministik + okumada anahtar dogrulamasi). Ayni API surumunde
  bellek paylasimi olmaz; bu bir hata degil, surekler arasi izin verilemeyecek bir seydir.
- `setDataDirectory` ile veri klasoru host'a ozel verilmezse varsayilan tek yer
  (`%LOCALAPPDATA%/prime/prime-xmap`) kullanilir ve ayarlar cakisir. Ayrica iki yoneticinin ayni
  disk onbellek klasorune ayni anda yazmasi guvenli degildir (ayni karo dosyalarina yarisma).
  Bu yuzden v0.26 ornekleri kendi klasorunu verir (orn. texture-host: eklenti klasoru).
- Disk onbellegini silmek calisan haritayi etkilemez: karolar once bellekte yasar; disk onbellegi
  yalnizca yeniden baslatma sonrasi ilk yuklemeyi hizlandirir.

### 7.4 Klasor secimi senaryolari (coklu harita / coklu uygulama)

| Ihtiyac | Ayar | Sonuc |
|---|---|---|
| Bir uygulama, n harita (mod fark etmez: Scenery/Vfr/Ifr/Dark) | dizin vermeye gerek yok | Her sey ortak; en hizli yol. Modlar goruntu stili, raster onbellegi ortak |
| Iki AYRI uygulama: ayarlar ayri, karolar ortak | `setDataDirectory` uygulama basina farkli + `setCacheDirectory` IKISINE AYNI | Ayarlar/cizimler izole, karo diski ortak (ikinci uygulama DSF taramaz, onbellekten okur) |
| Iki uygulama: hicbir sey paylasmasin | her ikisine farkli `setDataDirectory` | Tam izolasyon (UI test ornegi boyle) |
| Iki uygulama: ayarlar da ortak | hicbiri dizin set etmesin (varsayilan) | `prime_map.json` ortak (farkli deger yazan ezer); harita adlari farkli olmali |

Kritik ayrim:

- **Mod/style farki (Scenery vs IFR vs Dark) dizin gerektirmez**: modlar goruntu stilidir; diske
  yazilan karo ham veridir (sinif + yukseklik), stil/renk/blend goruntu uretiminde paletten uygulanir.
- **Canli senkron (birinde yapilan degisikligin digerine aninda islenmesi) dizin paylasimiyla
  OLMAZ**: ayarlar initialize'da bir kez okunur. Iki haritayi senkron tutmak host'un isidir:
  ayni degerleri iki haritaya da `setX` ile uygula; ayni yoneticide karo onbellegi zaten ortaktir.
- Ayni veri klasorunu paylasan iki host ayni harita adini kullanmamali (`maps/<ad>/` cakışır).

## 8. Katmanlar

Katmanlar `XMapLayer` enum'unda; grup/katman/alt katman hiyerarsisi vardir
(`NavaidsGroup > Vor/Ndb/Ils/Fixes`, `Runways > Paved/Unpaved/...`).

```cpp
mXMap->setIsLayerVisible(prime::XMapLayer::Roads, false);   // mod basina saklanir
mXMap->setIsLayerVisible(prime::XMapLayer::Fixes, true);
mXMap->showOnlyLayers({prime::XMapLayer::Vor, prime::XMapLayer::Ndb});
mXMap->resetLayersToDefaults();

mXMap->setLayerZoomRange(prime::XMapLayer::Fixes, 10.5, 99.0);        // 10.5'ten yakinda cizilir
mXMap->setLayerZoomRange(prime::XMapLayer::AirwaysGroup, 8.0, 99.0);  // grup: cocuklara yayilir

prime::LayerState st = mXMap->getLayerState(prime::XMapLayer::Runways);  // Off / On / Mixed
```

Hiyerarsi sorgusu (kendi panelini yazarsan): `parentOf`, `childrenOf`, `topLevelLayers`,
`kindOf`, `layerName`, `layerKey`, `isLayerDefaultVisible(layer, mode)` — hepsi manager'da.

## 9. Renkler ve zemin

```cpp
// Katman rengi (dogrudan tema rengi olan katmanlar; degilse false doner)
mXMap->setColor(prime::XMapLayer::AirwaysLow, prime::Rgb{255, 140, 0});
mXMap->setColor(prime::XMapLayer::Vor, prime::Rgb{220, 40, 40});
prime::Rgb c = mXMap->getColor(prime::XMapLayer::Vor);
mXMap->clearColor(prime::XMapLayer::Vor);            // tema rengine doner

// Zemin (get/set cifti; setTerrainSettings dokulari hemen yeni paletle ister)
prime::TerrainSettings t = mXMap->getTerrainSettings();
t.mShadingStrength    = 0.8;     // kabartma golgesi gucu
t.mVfrContourLines    = true;    // VFR bandi sinirlarinda kontur
t.mReliefExaggeration = 2.5;     // egim abartmasi
t.mCoastlineStrength  = 0.5;     // kiyi cizgisi
t.mBlendMeters        = 150.0;   // siniflar arasi yumusak gecis
mXMap->setTerrainSettings(t);
```

- `mUseXPlaneColors=false` + `mClassColors`: sinif renklerini elle ver (`"water"`, `"forest_conifer"`...).
- `mVfrBandsFeet`: VFR rakim bantlarini degistir (feet + RGB ciftleri).

## 10. Kamera / gezinme

```cpp
mXMap->setCenter(47.45, -122.30);       // takip aciksa otomatik kapanir
mXMap->setZoom(10.0);
mXMap->setZoomLimits(6.0, 15.0);        // harita bu araliga kilitlenir
mXMap->fitBounds(bounds);               // alani ekrana sigdirir
mXMap->setFollowAircraft(true);
mXMap->goToAirport("LTFM");             // bool; yoksa log + false
auto matches = mXMap->findAirports("LTF", 8);  // AirportMatch listesi
```

## 11. Veri kaynaklari, cizimler, sorgular

```cpp
// Ucak / rota verisini HOST vermek istersen:
mXMap->setAircraftSource(prime::DataSource::Host);
mXMap->setAircraft(state);            // AircraftState (pozisyon, heading, hiz, irtifa)
mXMap->setRouteSource(prime::DataSource::Host);
mXMap->setRoute(waypoints, activeIndex);

// Kullanici cizimleri (programatik):
mXMap->setDrawTool(prime::DrawTool::Route);   // Pan / Point / Route / Region
mXMap->addDrawingPoint(lat, lon);
mXMap->finishDrawing();
std::vector<prime::Shape> shapes = mXMap->getShapes();
mXMap->removeShape(id);
mXMap->exportDrawings();             // ayarlar klasorune yazar
mXMap->importDrawings(text);         // metinden geri yukler

// Sorgu ve donusum:
prime::LatLon p = mXMap->screenToGeo(x, y);    // doku boxel -> lat/lon
mXMap->geoToScreen(lat, lon, &x, &y);          // doner deger: alan icinde mi
auto objects = mXMap->queryAt(lat, lon);       // o yerdeki nesneler (acik pencere gerekir)
bool ready = mXMap->isDataLoaded();            // nav+havalimani+airway+zemin hazir mi
prime::Status s = mManager->getStatus();       // yukleme durumu, bekleyen doku, onbellek...
```

## 12. Onbellek ve teshis

```cpp
mXMap->precacheVisibleArea();                     // ekranda gorunen alani onbellekle
mXMap->precacheArea(bounds);                      // belirli alan
mXMap->cancelPrecache();
mXMap->setDiagnostics(true);                      // DIAG satirlari Log.txt'ye
```

## 13. Sik yapilan hatalar

1. **CRT uyusmazligi**: host `/MD` degil `/MT` ile derlenirse surek acilir/acilmaz coker. Ayni
   runtime zorunlu (`CMAKE_MSVC_RUNTIME_LIBRARY`).
2. **Ana thread kurali**: tum API cagrilari X-Plane ana thread'inden (cizim/flight loop callback).
   Gecersiz deger: no-op + Log uyari, `false`/bos donus.
2a. `update()` cagirmazsin: kutuphane kendi XPLM flight loop'unu kurar ve her karede haritalari
   gunceller (`XMapData::start` -> `onFlightLoop` -> `XMap::update`).
3. `initialize()` haritada **en son**; ozellestirmeler ondan sonra (yoksa `LoadAndSave` dosya okuyup
   ezer — bilerek: her acilista koddaki degerler kazanacaksa `customizeMap`'i initialize sonrasi tut).
4. Kapanis sirasi: `removeXMap` -> `shutdown` -> `destroyXMapManager` -> pencere.
5. Veri klasorunu host'a ozel ver; baska host ile ayar/onbellek paylasma.
6. C4251: `/W4` ile `/wd4251` ekle (pragma kullanilmaz).
7. Tum XPLM pencere callback'lerini doldur (hiçbiri null birakilmamali — SDK istegi).

## 14. API hiz basvurusu (konulara gore)

| Konu | Metotlar |
|---|---|
| Yasam dongusu | `addXMap`, `removeXMap`, `initialize`, `close`, `isInitialized` |
| Görünüm | `setStyle`, `setTitle`, `setGeometry`, `setVisible`, `bringToFront`, `setPoppedOut`, `setIsUiVisible` |
| Kamera | `setCenter`, `setZoom`, `setZoomLimits`, `zoomIn/Out`, `pan*`, `fitBounds`, `getVisibleBounds`, `getMetersPerBoxel` |
| Texture | `setTextureSize`, `renderTexture`, `getTextureId/Width/Height`, `injectMouse/Move/Wheel/Key`, `wantsKeyboard` |
| Katmanlar | `setIsLayerVisible`, `toggleLayer`, `showOnlyLayers`, `getLayerState`, `set/get/clearLayerZoomRange`, `resetLayersToDefaults` |
| Renkler | `setColor/getColor/clearColor/clearAllColors`, `getTerrainSettings`, `setTerrainSettings`, `exportTerrainSettings` |
| Veri kaynaklari | `setAircraftSource/RouteSource`, `setAircraft`, `setRoute`, `clearTrack` |
| Cizimler | `setDrawTool`, `addDrawingPoint`, `finishDrawing`, `undoDrawing`, `clearDrawings`, `getShapes`, `removeShape` |
| Sorgular | `findAirports`, `goToAirport`, `queryAt/AtScreen`, `screenToGeo`, `geoToScreen` |
| Durum | `isDataLoaded`, `getStatusText`, `getTerrainPendingImages`, `getSettingsRevision`, `getDrawingsRevision` |
| Onbellek | `precacheVisibleArea`, `precacheArea`, `cancelPrecache`, `setImageCache`, `setMaxRenderFps`, `setXPlaneFont` |
| Kalicilik | `saveSettings`, `setPersistence`, `getSettingsDirectory`, `exportSettings`, `importSettings` |
| Teshis | `setDiagnostics`, `isDiagnosticsOn`, `onRender`, `update` |

Kaynak ornek: `examples/texture_host/src/TextureHost.cpp` — en kucuk tam host.
Detayli arayuz: `PrimeMap.h` + `docs/PrimeMap_API.html`.
