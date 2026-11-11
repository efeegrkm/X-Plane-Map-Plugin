# plugin_api_test: Prime Map API'yi denemek icin AYRI bir X-Plane eklentisi.
#
#   cmake -S C:\workspace\projects\plugin-api-test -B <derleme> -G Ninja -DCMAKE_BUILD_TYPE=Release
#   ninja -C <derleme>   ->  <derleme>/plugin_api_test/64/win.xpl (+ prime_map.dll)
#
# Kutuphane ayni CMake oturumuna add_subdirectory ile katilir, hedef adina (prime_map)
# linklenir. Erisim yalnizca MapApiCalls sinifi uzerinden gecer (SDK_BOUND).
cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0091 NEW)
project(plugin_api_test LANGUAGES CXX)

set(COTS_DIR "C:/workspace/cots" CACHE PATH "Ucuncu taraf bileşen klasoru (COTS)")
set(XPSDK "${COTS_DIR}/X-Plane-SDK" CACHE PATH "X-Plane SDK klasoru")
# Prime Map kutuphanesinin reposu (kardes klasor)
set(PRIME_MAP_DIR "" CACHE PATH "prime_map_api klasoru (PrimeMap.h + CMakeLists.txt)")
if(NOT PRIME_MAP_DIR)
    set(PRIME_MAP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../prime_map_api")
endif()

if(NOT EXISTS "${XPSDK}/CHeaders/XPLM/XPLMDisplay.h" AND
   EXISTS "${COTS_DIR}/X-Plane-SDK/CHeaders/XPLM/XPLMDisplay.h")
    set(XPSDK "${COTS_DIR}/X-Plane-SDK" CACHE PATH "X-Plane SDK klasoru" FORCE)
endif()
if(NOT EXISTS "${PRIME_MAP_DIR}/PrimeMap.h")
    message(FATAL_ERROR "prime_map_api bulunamadi: ${PRIME_MAP_DIR}")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# CRT: /MD (prime_map ile ayni CRT zorunlu; STL tipleri DLL sinirindan geciyor).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# Kutuphaneyi ayni oturuma kat; hedef adina linkle
add_subdirectory(${PRIME_MAP_DIR} prime_map_build)

# SDK_BOUND: sarmalayici (MapApiCalls) derlemeye girdi; haritaya erisim sadece bu siniftan gecer.
# Basliklar da listede: Qt Creator proje agacinda gorunsun.
add_library(plugin_api_test SHARED
    src/MapUiTestMain.cpp
    src/MapApiCalls.h
    src/MapApiCalls.cpp)

target_include_directories(plugin_api_test SYSTEM PRIVATE ${XPSDK}/CHeaders/XPLM)
target_compile_definitions(plugin_api_test PRIVATE XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1
                       IBM=1 APL=0 LIN=0 _CRT_SECURE_NO_WARNINGS)
target_compile_options(plugin_api_test PRIVATE /W4 /utf-8 /permissive-)
target_link_libraries(plugin_api_test PRIVATE
    prime_map
    ${XPSDK}/Libraries/Win/XPLM_64.lib
    opengl32)
set_target_properties(plugin_api_test PROPERTIES PREFIX "" OUTPUT_NAME win SUFFIX ".xpl"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugin_api_test/64)

# prime_map.dll pluginin yanina kopyalanmali
add_custom_command(TARGET plugin_api_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:prime_map> ${CMAKE_BINARY_DIR}/plugin_api_test/64/)
