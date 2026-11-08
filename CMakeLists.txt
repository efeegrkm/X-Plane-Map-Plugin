cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0091 NEW)  # MSVC runtime secimi icin
project(prime_map_plugin VERSION 0.20 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# X-Plane SDK: projenin icinde third_party/XPSDK altinda geliyor
# (baska bir SDK kullanmak icin: -DXPSDK=<klasor>)
set(XPSDK "${CMAKE_SOURCE_DIR}/third_party/XPSDK" CACHE PATH "X-Plane SDK klasoru")

# X-Plane klasoru: derleme bitince plugin otomatik buraya kopyalanir
set(XPLANE_DIR "C:/X-Plane 11" CACHE PATH "X-Plane 11 klasoru")
option(MAP_DEPLOY "Derlemeden sonra plugini X-Plane'e kopyala" ON)
option(MAP_TESTS "X-Plane'siz testleri derle" OFF)
# Prime Map API (0.20): prime_map statik kutuphanesinin C calisma kutuphanesi.
# OFF = /MT (statik, prime_map_MT.lib), ON = /MD (DLL, prime_map_MD.lib). Debug'da sonuna "d".
option(PRIME_MAP_DYNAMIC_CRT "prime_map'i /MD ile derle (OFF: /MT)" OFF)
option(MAP_API_DEMO "Ornek host eklentisini (tools/map_api_demo) derle" ON)
option(MAP_DEPLOY_DEMO "Ornek host eklentisini de X-Plane'e kur" OFF)

if(NOT EXISTS "${XPSDK}/CHeaders/XPLM/XPLMDisplay.h")
    message(FATAL_ERROR "X-Plane SDK bulunamadi: ${XPSDK}\n"
                        "Dogru klasoru verin: -DXPSDK=<SDK klasoru>")
endif()

set(CMAKE_CXX_STANDARD 20)
# Butun hedefler C calisma kutuphanesini icine gomer (plugin ek DLL istemesin;
# lzma_sdk, testler ve araclar da ayni ayarla derlensin ki baglayici karismasin)
if(PRIME_MAP_DYNAMIC_CRT)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    set(PRIME_MAP_CRT MD)
else()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(PRIME_MAP_CRT MT)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# X-Plane'e bagimli OLMAYAN dosyalar: hem plugine hem testlere girer
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
    src/data/AptLine.cpp
    src/data/AptDatParser.cpp
    src/data/AptDatIndexer.cpp
    src/data/AirportShapes.cpp
    src/data/AirportDetailStore.cpp
)

# X-Plane SDK'si ve OpenGL kullanan dosyalar (prime_map kutuphanesine girer)
set(PLUGIN_SOURCES
    src/api/PrimeMap.cpp
    src/engine/MapData.cpp
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

# LZMA SDK (kamu mali): X-Plane DSF dosyalari 7z ile sikistirilmis (third_party/lzma/README.txt)
file(GLOB LZMA_SOURCES ${CMAKE_SOURCE_DIR}/third_party/lzma/*.c)
add_library(lzma_sdk OBJECT ${LZMA_SOURCES})  # nesneleri prime_map.lib'in icine girer
target_include_directories(lzma_sdk SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/third_party/lzma)
# Z7_ST: LZMA sikistiricisi tek thread'li derlenir (cok thread'li eslestirici LzFindMt eklenmedi;
# third_party/lzma/README.txt). Sikistirma zaten zemin iscisinin kendi thread'inde calisir.
target_compile_definitions(lzma_sdk PUBLIC Z7_ST)
set_property(TARGET lzma_sdk PROPERTY POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(lzma_sdk PRIVATE /W0)
else()
    target_compile_options(lzma_sdk PRIVATE -w)
endif()

# Ortak derleyici ayarlari (uyarilar acik; hem plugin hem test)
function(map_compile_options target)
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    # earcut.hpp (dis kutuphane, tek baslik): third_party/earcut/README.txt
    target_include_directories(${target} SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party/earcut)
    # stb_truetype.h (dis kutuphane, tek baslik): third_party/stb/README.txt
    target_include_directories(${target} SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(${target} PRIVATE Threads::Threads lzma_sdk)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8 /permissive-)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
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

# --- prime_map: teslim edilen statik kutuphane (Prime Map API, tek genel baslik src/api/PrimeMap.h)
add_library(prime_map STATIC ${CORE_SOURCES} ${PLUGIN_SOURCES})
map_compile_options(prime_map)
map_xplm_options(prime_map)
set_target_properties(prime_map PROPERTIES OUTPUT_NAME prime_map_${PRIME_MAP_CRT} DEBUG_POSTFIX d)

# Bir host eklentisini (win.xpl) prime_map'e baglar
function(map_host_plugin target)
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    map_xplm_options(${target})
    target_link_libraries(${target} PRIVATE prime_map Threads::Threads)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8 /permissive-)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
    if(WIN32)
        target_link_libraries(${target} PRIVATE ${XPSDK}/Libraries/Win/XPLM_64.lib opengl32)
    elseif(APPLE)
        target_link_libraries(${target} PRIVATE "-F${XPSDK}/Libraries/Mac" "-framework XPLM" "-framework OpenGL")
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(${target} PRIVATE OpenGL::GL)
    endif()
endfunction()

# Plugin aslinda bir DLL'dir (uzantisi .xpl): ince kabuk + prime_map
add_library(prime-map-plugin SHARED src/plugin/PluginMain.cpp)
map_host_plugin(prime-map-plugin)
if(WIN32)
    set(XPL_NAME win)
    if(NOT MSVC)
        target_link_options(prime-map-plugin PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
elseif(APPLE)
    set(XPL_NAME mac)
else()
    set(XPL_NAME lin)
endif()

# Cikti: build/prime-map-plugin/64/win.xpl
set(OUT_DIR ${CMAKE_BINARY_DIR}/prime-map-plugin)
set_target_properties(prime-map-plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME ${XPL_NAME}
    SUFFIX ".xpl"
    RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR}/64
    LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR}/64)
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${cfg} CFG)
    set_target_properties(prime-map-plugin PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CFG} ${OUT_DIR}/64
        LIBRARY_OUTPUT_DIRECTORY_${CFG} ${OUT_DIR}/64)
endforeach()

# Derleme bitince X-Plane'e kur:
#   <XPLANE_DIR>/Resources/plugins/prime-map-plugin/64/win.xpl
# Not: X-Plane aciksa win.xpl kilitli olur ve kopyalama basarisiz olur;
# once X-Plane'i kapat (veya Plugin Admin'den plugini devre disi birak).
if(MAP_DEPLOY AND EXISTS "${XPLANE_DIR}/Resources/plugins")
    set(DEPLOY_DIR "${XPLANE_DIR}/Resources/plugins/prime-map-plugin")
    add_custom_command(TARGET prime-map-plugin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/64"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:prime-map-plugin> "${DEPLOY_DIR}/64/"
        COMMENT "Plugin X-Plane'e kuruluyor: ${DEPLOY_DIR}")
endif()

# ---------------------------------------------------------------------------
# X-Plane'siz testler (istege bagli): cmake -DMAP_TESTS=ON
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
option(MAP_PREVIEW "X-Plane'siz goruntu onizleme aracini derle (Windows)" OFF)
if(MAP_PREVIEW AND WIN32)
    add_executable(map_preview
        tools/preview/PreviewMain.cpp
        tools/preview/XplmStubs.cpp
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
    target_include_directories(map_preview PRIVATE ${CMAKE_SOURCE_DIR})
    target_include_directories(map_preview SYSTEM PRIVATE ${XPSDK}/CHeaders/XPLM)
    target_compile_definitions(map_preview PRIVATE IBM=1 APL=0 LIN=0 XPLM=1
                               XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1)
    target_link_libraries(map_preview PRIVATE opengl32 gdi32 user32)

    # dsf_info: bir DSF dosyasinin ozeti (arazi turleri, ucgen sayilari)
    add_executable(dsf_info tools/dsfinfo/DsfInfoMain.cpp src/data/SevenZip.cpp src/data/DsfReader.cpp)
    map_compile_options(dsf_info)
endif()

# ---------------------------------------------------------------------------
# Ornek host eklentisi (GNS gelistiricisinin baslangic noktasi): tools/map_api_demo
if(MAP_API_DEMO)
    add_library(map_api_demo SHARED tools/map_api_demo/DemoMain.cpp)
    map_host_plugin(map_api_demo)
    target_include_directories(map_api_demo PRIVATE ${CMAKE_SOURCE_DIR}/src/api)  # host gibi: sadece PrimeMap.h
    set_target_properties(map_api_demo PROPERTIES PREFIX "" OUTPUT_NAME ${XPL_NAME} SUFFIX ".xpl"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/map_api_demo/64
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/map_api_demo/64
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/map_api_demo/lib)
    if(MAP_DEPLOY_DEMO AND EXISTS "${XPLANE_DIR}/Resources/plugins")
        add_custom_command(TARGET map_api_demo POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${XPLANE_DIR}/Resources/plugins/map_api_demo/64"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:map_api_demo>
                    "${XPLANE_DIR}/Resources/plugins/map_api_demo/64/"
            COMMENT "Demo X-Plane'e kuruluyor")
    endif()
endif()

# ---------------------------------------------------------------------------
# SDK paketi: cmake --install <derleme> --prefix <klasor>/prime_map_sdk [--config Debug|Release]
#   include/PrimeMap.h, lib/prime_map_MT[d].lib veya prime_map_MD[d].lib, docs, examples, CHANGELOG.md
# Iki CRT icin iki derleme klasoru gerekir (PRIME_MAP_DYNAMIC_CRT=OFF ve ON); ayni prefix'e kurulabilir.
install(TARGETS prime_map ARCHIVE DESTINATION lib)
install(FILES src/api/PrimeMap.h DESTINATION include)
install(FILES docs/CHANGELOG.md DESTINATION . OPTIONAL)
install(FILES docs/PrimeMap_API.pdf DESTINATION docs OPTIONAL)
install(DIRECTORY tools/map_api_demo/ DESTINATION examples/map_api_demo)
