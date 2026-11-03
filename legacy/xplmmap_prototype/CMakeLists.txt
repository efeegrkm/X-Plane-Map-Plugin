cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)  # MSVC runtime secimi icin
project(prime_map_plugin VERSION 0.3 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# X-Plane SDK: projenin icinde third_party/XPSDK altinda geliyor
# (baska bir SDK kullanmak icin: -DXPSDK=<klasor>)
set(XPSDK "${CMAKE_SOURCE_DIR}/third_party/XPSDK" CACHE PATH "X-Plane SDK klasoru")

# X-Plane klasoru: derleme bitince plugin otomatik buraya kopyalanir
set(XPLANE_DIR "C:/X-Plane 11" CACHE PATH "X-Plane 11 klasoru")
option(HARITA_DEPLOY "Derlemeden sonra plugini X-Plane'e kopyala" ON)
if(NOT EXISTS "${XPSDK}/CHeaders/XPLM/XPLMMap.h")
    message(FATAL_ERROR "X-Plane SDK bulunamadi: ${XPSDK}\n"
                        "Dogru klasoru verin: -DXPSDK=<SDK klasoru>")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    src/Plugin.cpp
    src/MapLayer.cpp
    src/Overlay.cpp
    src/ClickWindow.cpp
    src/ButtonPanel.cpp
    src/Drawings.cpp
    src/ScreenMath.cpp
    src/ScreenState.cpp
    src/Geo.cpp
    src/Log.cpp
)

# Plugin aslinda bir DLL'dir (uzantisi .xpl)
add_library(HaritaPlugin SHARED ${SOURCES})

target_include_directories(HaritaPlugin PRIVATE ${XPSDK}/CHeaders/XPLM)

# X-Plane 11 icin SDK 3.0 (XPLM300/301) ozellikleri; daha yenileri kapali
target_compile_definitions(HaritaPlugin PRIVATE XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1)

if(WIN32)
    target_compile_definitions(HaritaPlugin PRIVATE IBM=1 APL=0 LIN=0 _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(HaritaPlugin PRIVATE ${XPSDK}/Libraries/Win/XPLM_64.lib opengl32)
    set(XPL_NAME win)
    if(MSVC)
        # C++ calisma kutuphanesini icine gom (ek DLL gerekmesin)
        set_property(TARGET HaritaPlugin PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(HaritaPlugin PRIVATE /W3 /utf-8)
    else()
        # MinGW: libstdc++/libgcc'yi gom
        target_link_options(HaritaPlugin PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
elseif(APPLE)
    target_compile_definitions(HaritaPlugin PRIVATE IBM=0 APL=1 LIN=0)
    target_link_libraries(HaritaPlugin PRIVATE "-F${XPSDK}/Libraries/Mac" "-framework XPLM" "-framework OpenGL")
    set(XPL_NAME mac)
else()
    target_compile_definitions(HaritaPlugin PRIVATE IBM=0 APL=0 LIN=1)
    find_package(OpenGL REQUIRED)
    target_link_libraries(HaritaPlugin PRIVATE OpenGL::GL)
    set(XPL_NAME lin)
endif()

# Cikti: build/HaritaPlugin/64/win.xpl  (+ ikon: build/HaritaPlugin/harita_ikon.png)
set(OUT_DIR ${CMAKE_BINARY_DIR}/HaritaPlugin)
set_target_properties(HaritaPlugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME ${XPL_NAME}
    SUFFIX ".xpl"
    RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR}/64
    LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR}/64)
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${cfg} CFG)
    set_target_properties(HaritaPlugin PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CFG} ${OUT_DIR}/64
        LIBRARY_OUTPUT_DIRECTORY_${CFG} ${OUT_DIR}/64)
endforeach()

add_custom_command(TARGET HaritaPlugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/resources/harita_ikon.png ${OUT_DIR}/harita_ikon.png)

# Derleme bitince X-Plane'e kur:
#   <XPLANE_DIR>/Resources/plugins/HaritaPlugin/64/win.xpl  + harita_ikon.png
# Not: X-Plane aciksa win.xpl kilitli olur ve kopyalama basarisiz olur;
# once X-Plane'i kapat (veya Plugin Admin'den plugini devre disi birak).
if(HARITA_DEPLOY AND EXISTS "${XPLANE_DIR}/Resources/plugins")
    set(DEPLOY_DIR "${XPLANE_DIR}/Resources/plugins/HaritaPlugin")
    add_custom_command(TARGET HaritaPlugin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/64"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:HaritaPlugin> "${DEPLOY_DIR}/64/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/resources/harita_ikon.png "${DEPLOY_DIR}/harita_ikon.png"
        COMMENT "Plugin X-Plane'e kuruluyor: ${DEPLOY_DIR}")
endif()

# ---------------------------------------------------------------------------
# X-Plane'siz testler (istege bagli): cmake -DHARITA_TESTS=ON
option(HARITA_TESTS "X-Plane'siz testleri derle" OFF)
if(HARITA_TESTS)
    add_executable(harita_tests tests/tests.cpp src/Geo.cpp src/ScreenMath.cpp src/Drawings.cpp)
endif()
