cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0091 NEW)
project(prime-map-api VERSION 0.26.0 LANGUAGES C CXX)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/api/XMapTypes.h" _pm_ver REGEX "constexpr int kVersion(Major|Minor|Patch) *= *[0-9]+")
foreach(_pm_m IN ITEMS Major Minor Patch)
    string(REGEX MATCH "kVersion${_pm_m} *= *([0-9]+)" _pm_v "${_pm_ver}")
    if(NOT _pm_v)
        message(FATAL_ERROR "XMapTypes.h icinde kVersion${_pm_m} sabiti bulunamadi")
    endif()
    list(APPEND _pm_h "${CMAKE_MATCH_1}")
endforeach()
string(JOIN "." _pm_h_d ${_pm_h})
string(JOIN "." _pm_h_d ${_pm_h})
set(_pm_cm "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
if(NOT _pm_h_d STREQUAL _pm_cm)
    message(FATAL_ERROR "Surum uyusmazligi: XMapTypes.h kVersion = ${_pm_h_d}, CMake project(VERSION) = ${_pm_cm}")
endif()
unset(_pm_ver)
unset(_pm_h)
unset(_pm_h_d)
unset(_pm_h_d)
unset(_pm_cm)
unset(_pm_m)
unset(_pm_v)

set(COTS_DIR "C:/workspace/cots" CACHE PATH "Ucuncu taraf bilesen klasoru (COTS)")
if(NOT EXISTS "${COTS_DIR}/X-Plane-SDK" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../cots/X-Plane-SDK")
    get_filename_component(_pm_cots "${CMAKE_CURRENT_SOURCE_DIR}/../cots" ABSOLUTE)
    set(COTS_DIR "${_pm_cots}" CACHE PATH "Ucuncu taraf bilesen klasoru (COTS)" FORCE)
    unset(_pm_cots)
endif()
set(XPSDK "${COTS_DIR}/X-Plane-SDK" CACHE PATH "X-Plane SDK klasoru")
if(NOT EXISTS "${XPSDK}/CHeaders/XPLM/XPLMDisplay.h" AND EXISTS "${COTS_DIR}/X-Plane-SDK/CHeaders/XPLM/XPLMDisplay.h")
    set(XPSDK "${COTS_DIR}/X-Plane-SDK" CACHE PATH "X-Plane SDK klasoru" FORCE)
endif()
if(NOT EXISTS "${XPSDK}/CHeaders/XPLM/XPLMDisplay.h")
    message(FATAL_ERROR "X-Plane SDK bulunamadi: ${XPSDK}")
endif()

option(MAP_TESTS "X-Plane'siz testleri derle" OFF)
option(MAP_PREVIEW "X-Plane'siz goruntu onizleme aracini derle (Windows)" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

set(CORE_SOURCES
    src/Geo.cpp src/Drawings.cpp src/ScreenMath.cpp src/XMapTheme.cpp src/XMapView.cpp src/XMapFormat.cpp
    src/Settings.cpp src/LayerManager.cpp src/LabelPlacer.cpp src/GeoGrid.cpp src/BackgroundJob.cpp
    src/engine/LayerTable.cpp
    src/gfx/FontAtlas.cpp src/gfx/StbTruetypeImpl.cpp src/gfx/Mesh.cpp
    src/data/NavDatabase.cpp src/data/AirportGeometry.cpp src/data/AirportGeometryStore.cpp
    src/data/AirportSearch.cpp src/data/LocalizerMatch.cpp src/data/AirwayDatabase.cpp
    src/data/SevenZip.cpp src/data/DsfReader.cpp src/data/SceneryPacks.cpp src/data/SceneryIndex.cpp
    src/data/TerrainClasses.cpp src/data/TerrainVectors.cpp src/data/TerrainPaletteSource.cpp
    src/data/WorldTileMap.cpp src/data/ElevationCodec.cpp src/data/TerrainElevation.cpp
    src/data/TerrainRelief.cpp src/data/LandColorField.cpp src/data/TerrainNoise.cpp
    src/data/TerrainRaster.cpp src/data/TerrainTriangles.cpp src/data/CacheTrim.cpp
    src/data/TerrainStore.cpp src/data/TerrainProfile.cpp src/data/AptLine.cpp
    src/data/AptDatParser.cpp src/data/AptDatIndexer.cpp src/data/AirportShapes.cpp
    src/data/AirportDetailStore.cpp
)

set(PLUGIN_SOURCES
    src/api/XMap.cpp src/api/XMapManager.cpp src/api/XMapManagerFactory.cpp
    src/engine/XMapData.cpp
    src/XMapWindow.cpp src/Log.cpp
    src/data/AircraftData.cpp src/data/NavLoader.cpp
    src/gfx/GlApi.cpp src/gfx/GlScreen.cpp src/gfx/Canvas.cpp src/gfx/ContextVertexArray.cpp
    src/gfx/StaticMesh.cpp src/gfx/Texture.cpp src/gfx/RenderTarget.cpp
    src/XMapImageCache.cpp
    src/layers/XMapSymbols.cpp src/layers/TerrainHub.cpp src/layers/TerrainLayer.cpp
    src/layers/RoadLayer.cpp src/layers/AirportSurfaceLayer.cpp src/layers/AirportObjectsLayer.cpp
    src/layers/TaxiRouteLayer.cpp src/layers/AirportVisibility.cpp src/layers/GridLayer.cpp
    src/layers/RunwayLayer.cpp src/layers/AirwayLayer.cpp src/layers/AirportLayer.cpp
    src/layers/NavaidLayer.cpp src/layers/FmsLayer.cpp src/layers/TrackLayer.cpp
    src/layers/AircraftLayer.cpp src/layers/DrawingLayer.cpp
    src/ui/UiDraw.cpp src/ui/Toolbar.cpp src/ui/LayerPanel.cpp src/ui/ToolPalette.cpp
    src/ui/ModePanel.cpp src/ui/AirportSearchPanel.cpp src/ui/AtlasTextRenderer.cpp
    src/ui/StatusBar.cpp
)

find_package(Threads REQUIRED)

file(GLOB LZMA_SOURCES ${COTS_DIR}/lzma/*.c)
add_library(lzma_sdk OBJECT ${LZMA_SOURCES})
target_include_directories(lzma_sdk SYSTEM PUBLIC ${COTS_DIR}/lzma)
target_compile_definitions(lzma_sdk PUBLIC Z7_ST)
set_property(TARGET lzma_sdk PROPERTY POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(lzma_sdk PRIVATE /W0)
else()
    target_compile_options(lzma_sdk PRIVATE -w)
endif()

add_library(${PROJECT_NAME} SHARED ${CORE_SOURCES} ${PLUGIN_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${COTS_DIR}/earcut ${COTS_DIR})
target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${XPSDK}/CHeaders/XPLM)
target_compile_definitions(${PROJECT_NAME} PRIVATE PRIME_MAP_LIBRARY XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1
                           _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE IBM=1 APL=0 LIN=0)
elseif(APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE IBM=0 APL=1 LIN=0)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE IBM=0 APL=0 LIN=1)
endif()
target_compile_options(${PROJECT_NAME} PRIVATE /W4 /wd4251 /utf-8 /permissive-)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads lzma_sdk opengl32 gdi32 user32)
target_link_libraries(${PROJECT_NAME} PRIVATE ${XPSDK}/Libraries/Win/XPLM_64.lib)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME prime_map)

file(GLOB_RECURSE MAP_HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
target_sources(${PROJECT_NAME} PRIVATE
    ${MAP_HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/PrimeMap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/primemapglobal.h)

if(WIN32)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/primemap.rc.in
                   ${CMAKE_CURRENT_BINARY_DIR}/primemap.rc @ONLY)
    target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/primemap.rc)
endif()

if(MAP_TESTS)
    add_executable(map_tests tests/tests.cpp ${CORE_SOURCES})
    target_include_directories(map_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories(map_tests SYSTEM PRIVATE ${COTS_DIR}/earcut ${COTS_DIR})
    target_compile_definitions(map_tests PRIVATE PRIME_MAP_LIBRARY
                               _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(map_tests PRIVATE /W4 /wd4251 /utf-8 /permissive-)
    else()
        target_compile_options(map_tests PRIVATE -Wall -Wextra)
    endif()
    target_link_libraries(map_tests PRIVATE Threads::Threads lzma_sdk)
    enable_testing()
    add_test(NAME map_tests COMMAND map_tests)
endif()

if(MAP_PREVIEW AND WIN32)
    add_executable(map_preview
        tools/preview/PreviewMain.cpp tools/preview/XplmStubs.h tools/preview/XplmStubs.cpp
        tools/preview/FileNavLoader.h tools/preview/FileNavLoader.cpp
        ${CORE_SOURCES}
        src/Log.cpp src/data/AircraftData.cpp
        src/gfx/GlApi.cpp src/gfx/GlScreen.cpp src/gfx/Canvas.cpp src/gfx/ContextVertexArray.cpp
        src/gfx/StaticMesh.cpp src/gfx/Texture.cpp src/gfx/RenderTarget.cpp
        src/XMapImageCache.cpp
        src/layers/XMapSymbols.cpp src/layers/TerrainHub.cpp src/layers/TerrainLayer.cpp
        src/layers/RoadLayer.cpp src/layers/AirportSurfaceLayer.cpp src/layers/AirportObjectsLayer.cpp
        src/layers/TaxiRouteLayer.cpp src/layers/AirportVisibility.cpp src/layers/GridLayer.cpp
        src/layers/RunwayLayer.cpp src/layers/AirwayLayer.cpp src/layers/AirportLayer.cpp
        src/layers/NavaidLayer.cpp src/layers/AircraftLayer.cpp src/layers/DrawingLayer.cpp
        src/ui/UiDraw.cpp src/ui/Toolbar.cpp src/ui/LayerPanel.cpp src/ui/ToolPalette.cpp
        src/ui/ModePanel.cpp src/ui/StatusBar.cpp src/ui/AtlasTextRenderer.cpp)
    target_include_directories(map_preview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories(map_preview SYSTEM PRIVATE ${COTS_DIR}/earcut ${COTS_DIR} ${XPSDK}/CHeaders/XPLM)
    target_compile_definitions(map_preview PRIVATE PRIME_MAP_LIBRARY IBM=1 APL=0 LIN=0 XPLM=1
                               XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1
                               _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(map_preview PRIVATE /W4 /wd4251 /utf-8 /permissive-)
    target_link_libraries(map_preview PRIVATE Threads::Threads lzma_sdk opengl32 gdi32 user32)

    add_executable(dsf_info tools/dsfinfo/DsfInfoMain.cpp)
    target_include_directories(dsf_info PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
    target_include_directories(dsf_info SYSTEM PRIVATE ${COTS_DIR})
    target_compile_definitions(dsf_info PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(dsf_info PRIVATE /W4 /wd4251 /utf-8 /permissive-)
    target_link_libraries(dsf_info PRIVATE Threads::Threads lzma_sdk)
    target_sources(dsf_info PRIVATE src/data/SevenZip.cpp src/data/DsfReader.cpp)
endif()

# stb_truetype implementasyonu: harici kutuphane, tek dosyada uyarilar kapali
if(MSVC)
    set_source_files_properties(src/gfx/StbTruetypeImpl.cpp PROPERTIES COMPILE_OPTIONS /W0)
else()
    set_source_files_properties(src/gfx/StbTruetypeImpl.cpp PROPERTIES COMPILE_OPTIONS -w)
endif()
