cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0091 NEW)  # MSVC runtime secimi icin
project(prime_map_api VERSION 0.23 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Surum denetimi: genel basliktaki kVersionMajor/Minor/Patch sabitleri
# project(VERSION) ile ayni olmalidir; uyusmazlik configure hatasidir.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/PrimeMap.h" _pm_ver REGEX "constexpr int kVersion(Major|Minor|Patch) *= *[0-9]+")
set(_pm_h "")
foreach(_m IN ITEMS Major Minor Patch)
    string(REGEX MATCH "kVersion${_m} *= *([0-9]+)" _pm_m "${_pm_ver}")
    if(NOT _pm_m)
        message(FATAL_ERROR "PrimeMap.h icinde kVersion${_m} sabiti bulunamadi")
    endif()
    list(APPEND _pm_h "${CMAKE_MATCH_1}")
endforeach()
string(JOIN "." _pm_h_d ${_pm_h})
set(_pm_patch "${PROJECT_VERSION_PATCH}")
if(_pm_patch STREQUAL "")
    set(_pm_patch "0")  # project(VERSION x.y) -> patch belirtilmemis
endif()
set(_pm_cm "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${_pm_patch}")
if(NOT _pm_h_d STREQUAL _pm_cm)
    message(FATAL_ERROR "Surum uyusmazligi: PrimeMap.h kVersion = ${_pm_h_d}, "
                        "CMake project(VERSION) = ${_pm_cm}. Ikisini birlikte guncelleyin.")
endif()
unset(_pm_ver)
unset(_pm_h)
unset(_pm_h_d)
unset(_pm_cm)
unset(_pm_patch)
unset(_pm_m)

# ---------------------------------------------------------------------------
# COTS: ucuncu taraf bileşenler repoda TUTULMAZ; C:\workspace\cots altindan okunur.
# Kurulum ve indirme linkleri: README.md -> "Ucuncu Taraf Bilesenler"
set(COTS_DIR "C:/workspace/cots" CACHE PATH "Ucuncu taraf bileşen klasoru (COTS)")

# COTS klasoru varsayilan yerde yoksa kardes "cots" klasorune bak (ornek: C:/Projects/QT/cots).
# Sirket bilgisayarinda C:/workspace/cots vardir ve bu blok hicbir sey yapmaz.
if(NOT EXISTS "${COTS_DIR}/X-Plane-SDK" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../cots/X-Plane-SDK")
    get_filename_component(_pm_cots "${CMAKE_CURRENT_SOURCE_DIR}/../cots" ABSOLUTE)
    set(COTS_DIR "${_pm_cots}" CACHE PATH "Ucuncu taraf bileşen klasoru (COTS)" FORCE)
    unset(_pm_cots)
endif()

# X-Plane SDK: <COTS>/X-Plane-SDK
# (baska bir SDK kullanmak icin: -DXPSDK=<klasor>)
set(XPSDK "${COTS_DIR}/X-Plane-SDK" CACHE PATH "X-Plane SDK klasoru")

option(MAP_TESTS "X-Plane'siz testleri derle" OFF)
option(MAP_PREVIEW "X-Plane'siz goruntu onizleme aracini derle (Windows)" OFF)

# Eski derleme klasorunde kalmis bayat XPSDK yolunu kendiliginden duzelt
if(NOT EXISTS "${XPSDK}/CHeaders/XPLM/XPLMDisplay.h" AND
   EXISTS "${COTS_DIR}/X-Plane-SDK/CHeaders/XPLM/XPLMDisplay.h")
    set(XPSDK "${COTS_DIR}/X-Plane-SDK" CACHE PATH "X-Plane SDK klasoru" FORCE)
endif()

if(NOT EXISTS "${XPSDK}/CHeaders/XPLM/XPLMDisplay.h")
    message(FATAL_ERROR "X-Plane SDK bulunamadi: ${XPSDK}\n"
                        "Kurulum: README.md -> Ucuncu Taraf Bilesenler\n"
                        "Dogru klasoru vermek icin: -DXPSDK=<SDK klasoru>")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# CRT: /MD (dinamik). STL tipleri DLL sinirindan gectigi icin tuketiciler de
# ayni CRT ile derlenmek zorundadir.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# X-Plane'e bagimli OLMAYAN dosyalar: hem kutuphaneye hem testlere girer
set(CORE_SOURCES
    src/Geo.cpp
    src/Drawings.cpp
    src/ScreenMath.cpp
    src/MapTheme.cpp
    src/MapView.cpp
    src/MapFormat.cpp
    src/Settings.cpp
    src/LayerManager.cpp
    src/engine/LayerTable.cpp
    src/LabelPlacer.cpp
    src/GeoGrid.cpp
    src/BackgroundJob.cpp
    src/gfx/FontAtlas.cpp
    src/gfx/Mesh.cpp
    src/data/NavDatabase.cpp
    src/data/AirportGeometry.cpp
    src/data/AirportGeometryStore.cpp
    src/data/AirportSearch.cpp
    src/data/LocalizerMatch.cpp
    src/data/AirwayDatabase.cpp
    src/data/SevenZip.cpp
    src/data/DsfReader.cpp
    src/data/SceneryPacks.cpp
    src/data/SceneryIndex.cpp
    src/data/TerrainClasses.cpp
    src/data/TerrainVectors.cpp
    src/data/TerrainPaletteSource.cpp
    src/data/WorldTileMap.cpp
    src/data/ElevationCodec.cpp
    src/data/TerrainElevation.cpp
    src/data/TerrainRelief.cpp
    src/data/LandColorField.cpp
    src/data/TerrainNoise.cpp
    src/data/TerrainRaster.cpp
    src/data/TerrainTriangles.cpp
    src/data/CacheTrim.cpp
    src/data/TerrainStore.cpp
    src/data/TerrainProfile.cpp
    src/data/AptLine.cpp
    src/data/AptDatParser.cpp
    src/data/AptDatIndexer.cpp
    src/data/AirportShapes.cpp
    src/data/AirportDetailStore.cpp
)

# X-Plane SDK'si ve OpenGL kullanan dosyalar (prime_map DLL'ine girer)
set(PLUGIN_SOURCES
    src/api/PrimeMap.cpp
    src/engine/MapData.cpp
    src/engine/MapInstance.cpp
    src/MapWindow.cpp
    src/Log.cpp
    src/data/AircraftData.cpp
    src/data/NavLoader.cpp
    src/gfx/GlApi.cpp
    src/gfx/GlScreen.cpp
    src/gfx/Canvas.cpp
    src/gfx/ContextVertexArray.cpp
    src/gfx/StaticMesh.cpp
    src/gfx/Texture.cpp
    src/gfx/RenderTarget.cpp
    src/MapImageCache.cpp
    src/layers/MapSymbols.cpp
    src/layers/TerrainHub.cpp
    src/layers/TerrainLayer.cpp
    src/layers/RoadLayer.cpp
    src/layers/AirportSurfaceLayer.cpp
    src/layers/AirportObjectsLayer.cpp
    src/layers/TaxiRouteLayer.cpp
    src/layers/AirportVisibility.cpp
    src/layers/GridLayer.cpp
    src/layers/RunwayLayer.cpp
    src/layers/AirwayLayer.cpp
    src/layers/AirportLayer.cpp
    src/layers/NavaidLayer.cpp
    src/layers/FmsLayer.cpp
    src/layers/TrackLayer.cpp
    src/layers/AircraftLayer.cpp
    src/layers/DrawingLayer.cpp
    src/ui/UiDraw.cpp
    src/ui/Toolbar.cpp
    src/ui/LayerPanel.cpp
    src/ui/ToolPalette.cpp
    src/ui/ModePanel.cpp
    src/ui/AirportSearchPanel.cpp
    src/ui/AtlasTextRenderer.cpp
    src/ui/StatusBar.cpp
)

# Pist verisi arka plan thread'inde okunur (std::thread)
find_package(Threads REQUIRED)

# LZMA SDK: X-Plane DSF dosyalari 7z ile sikistirilmis (cots/lzma/README.txt)
file(GLOB LZMA_SOURCES ${COTS_DIR}/lzma/*.c)
add_library(lzma_sdk OBJECT ${LZMA_SOURCES})  # nesneleri prime_map.dll'in icine girer
target_include_directories(lzma_sdk SYSTEM PUBLIC ${COTS_DIR}/lzma)
# Z7_ST: LZMA sikistiricisi tek thread'li derlenir (cok thread'li eslestirici LzFindMt eklenmedi;
# cots/lzma/README.txt). Sikistirma zaten zemin iscisinin kendi thread'inde calisir.
target_compile_definitions(lzma_sdk PUBLIC Z7_ST)
set_property(TARGET lzma_sdk PROPERTY POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(lzma_sdk PRIVATE /W0)
else()
    target_compile_options(lzma_sdk PRIVATE -w)
endif()

# Ortak derleyici ayarlari (uyarilar acik; hem kutuphane hem test)
function(map_compile_options target)
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR})  # genel basliklar repo kokunde (PrimeMap.h, primemapglobal.h)
    # earcut.hpp (dis kutuphane, tek baslik): cots/earcut/README.txt
    target_include_directories(${target} SYSTEM PRIVATE ${COTS_DIR}/earcut)
    # stb_truetype.h (dis kutuphane, tek baslik): cots/stb/README.txt
    target_include_directories(${target} SYSTEM PRIVATE ${COTS_DIR})
    target_link_libraries(${target} PRIVATE Threads::Threads lzma_sdk)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8 /permissive-)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
    # Kütüphane kaynaklarını derleyen her hedef (prime_map, map_tests, map_preview)
    # export tarafını derler: PRIME_MAP_EXPORT dllexport olur ve iç sınıflar
    # uyarısız derlenir (map_tests DLL'i kullanmaz, iç sınıfları doğrudan test eder).
    target_compile_definitions(${target} PRIVATE PRIME_MAP_LIBRARY)
endfunction()

# SDK basliklari ve tanimlari (X-Plane 11: SDK 3.0; XP12'ye ozel API derleme hatasi verir)
function(map_xplm_options target)
    target_include_directories(${target} SYSTEM PRIVATE ${XPSDK}/CHeaders/XPLM)
    target_compile_definitions(${target} PRIVATE XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1)
    if(WIN32)
        target_compile_definitions(${target} PRIVATE IBM=1 APL=0 LIN=0)
    elseif(APPLE)
        target_compile_definitions(${target} PRIVATE IBM=0 APL=1 LIN=0)
    else()
        target_compile_definitions(${target} PRIVATE IBM=0 APL=0 LIN=1)
    endif()
endfunction()

# --- prime_map ---------------------------------------------------------------
# SHARED kutuphane. Genel yuzey yalnizca PrimeMap.h'deki PRIME_MAP_EXPORT'lu
# sembollerdir; geri kalan her simge DLL icinde kalir. Paket uretilmez (install
# yok): tuketici ayni CMake oturumuna add_subdirectory ile katilir ve hedef
# adina linkler:
#   add_subdirectory(<prime_map_api yolu> prime_map_build)
#   target_link_libraries(<hedef> PRIVATE prime_map)
add_library(prime_map SHARED ${CORE_SOURCES} ${PLUGIN_SOURCES})
map_compile_options(prime_map)
map_xplm_options(prime_map)
if(WIN32)
    # XPLM_64.lib: sembolleri X-Plane.exe'nin cikardigi import kutuphanesi. DLL
    # yuklenirken X-Plane tarafindan cozulur (win.xpl ile ayni yontem).
    target_link_libraries(prime_map PRIVATE ${XPSDK}/Libraries/Win/XPLM_64.lib opengl32 gdi32 user32)
endif()

# Qt Creator proje agacinda baslik dosyalarini da goster
file(GLOB_RECURSE MAP_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
target_sources(prime_map PRIVATE
    ${MAP_HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/PrimeMap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/primemapglobal.h)

# Tuketici include'u klasor adi onekli yazilir: #include <prime_map_api/PrimeMap.h>
target_include_directories(prime_map
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Windows surum kaynagi: surum bilgisi CMake PROJECT_VERSION'dan DLL'e gomulur
# (Dosya Ozellikleri -> Ayrintilar).
if(WIN32)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/primemap.rc.in
                   ${CMAKE_CURRENT_BINARY_DIR}/primemap.rc @ONLY)
    target_sources(prime_map PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/primemap.rc)
endif()

# ---------------------------------------------------------------------------
# X-Plane'siz testler (istege bagli): cmake -DMAP_TESTS=ON
# (Testler ic siniflara erisir; kutuphane kaynagini dogrudan derler, DLL'i kullanmaz.)
if(MAP_TESTS)
    add_executable(map_tests tests/tests.cpp ${CORE_SOURCES})
    map_compile_options(map_tests)
    enable_testing()
    add_test(NAME map_tests COMMAND map_tests)
endif()

# ---------------------------------------------------------------------------
# Onizleme araci (istege bagli, sadece Windows): cmake -DMAP_PREVIEW=ON
# Katmanlari X-Plane olmadan, gercek X-Plane verisiyle bir resme cizer
# (tools/preview/PreviewMain.cpp). SDK fonksiyonlarinin karsiliklari
# XplmStubs.cpp'de; XPLM=1 ile SDK basliklari onlari "bizim" sayar.
if(MAP_PREVIEW AND WIN32)
    add_executable(map_preview
        tools/preview/PreviewMain.cpp
        tools/preview/XplmStubs.h
        tools/preview/XplmStubs.cpp
        tools/preview/FileNavLoader.h
        tools/preview/FileNavLoader.cpp
        ${CORE_SOURCES}
        src/Log.cpp
        src/data/AircraftData.cpp
        src/gfx/GlApi.cpp
        src/gfx/GlScreen.cpp
        src/gfx/Canvas.cpp
        src/gfx/ContextVertexArray.cpp
        src/gfx/StaticMesh.cpp
        src/gfx/Texture.cpp
        src/gfx/RenderTarget.cpp
        src/MapImageCache.cpp
        src/layers/MapSymbols.cpp
        src/layers/TerrainHub.cpp
        src/layers/TerrainLayer.cpp
        src/layers/RoadLayer.cpp
        src/layers/AirportSurfaceLayer.cpp
        src/layers/AirportObjectsLayer.cpp
        src/layers/TaxiRouteLayer.cpp
        src/layers/AirportVisibility.cpp
        src/layers/GridLayer.cpp
        src/layers/RunwayLayer.cpp
        src/layers/AirwayLayer.cpp
        src/layers/AirportLayer.cpp
        src/layers/NavaidLayer.cpp
        src/layers/AircraftLayer.cpp
        src/layers/DrawingLayer.cpp
        src/ui/UiDraw.cpp
        src/ui/Toolbar.cpp
        src/ui/LayerPanel.cpp
        src/ui/ToolPalette.cpp
        src/ui/ModePanel.cpp
        src/ui/StatusBar.cpp
        src/ui/AtlasTextRenderer.cpp)
    map_compile_options(map_preview)
    target_include_directories(map_preview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories(map_preview SYSTEM PRIVATE ${XPSDK}/CHeaders/XPLM)
    target_compile_definitions(map_preview PRIVATE IBM=1 APL=0 LIN=0 XPLM=1
                               XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1)
    target_link_libraries(map_preview PRIVATE opengl32 gdi32 user32)

    # dsf_info: bir DSF dosyasinin ozeti (arazi turleri, ucgen sayilari)
    add_executable(dsf_info tools/dsfinfo/DsfInfoMain.cpp src/data/SevenZip.cpp src/data/DsfReader.cpp)
    map_compile_options(dsf_info)
endif()

# NOT: install()/paket uretimi yok. Tuketim yukaridaki add_subdirectory +
# hedef adina link yontemidir.
