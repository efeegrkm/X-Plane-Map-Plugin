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
# COTS klasoru varsayilan yerde yoksa kardes "cots" klasorune bak (ornek: C:/Projects/QT/cots).
# Sirket bilgisayarinda C:/workspace/cots vardir ve bu blok hicbir sey yapmaz.
if(NOT EXISTS "${COTS_DIR}/X-Plane-SDK" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../cots/X-Plane-SDK")
    get_filename_component(_pm_cots "${CMAKE_CURRENT_SOURCE_DIR}/../cots" ABSOLUTE)
    set(COTS_DIR "${_pm_cots}" CACHE PATH "Ucuncu taraf bileşen klasoru (COTS)" FORCE)
    unset(_pm_cots)
endif()
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
    src/MapApiCalls.cpp
    src/MultiMapSelfTest.h
    src/MultiMapSelfTest.cpp)

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

# Istege bagli: derleme bitince X-Plane'e kur (-DMAP_DEPLOY=ON; X-Plane aciksa dosya kilitlidir).
# Varsayilan KAPALI: test eklentisi sadece istenince kurulur.
set(XPLANE_DIR "C:/X-Plane 11" CACHE PATH "X-Plane 11 klasoru")
option(MAP_DEPLOY "Derlemeden sonra test eklentisini X-Plane'e kopyala" OFF)
if(MAP_DEPLOY AND EXISTS "${XPLANE_DIR}/Resources/plugins")
    set(DEPLOY_DIR "${XPLANE_DIR}/Resources/plugins/plugin-api-test")
    add_custom_command(TARGET plugin_api_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/64"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:plugin_api_test> "${DEPLOY_DIR}/64/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:prime_map> "${DEPLOY_DIR}/64/"
        COMMENT "Test eklentisi X-Plane'e kuruluyor: ${DEPLOY_DIR}")
endif()

# prime_map.dll'nin kopyalari HER derlemede yenilenir (POST_BUILD sadece win.xpl yeniden baglaninca
# calisir; yalnizca kutuphane degistiginde bayat DLL kalirdi).
add_custom_target(plugin_api_test_dll ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:prime_map> ${CMAKE_BINARY_DIR}/plugin_api_test/64/
    DEPENDS prime_map plugin_api_test
    COMMENT "prime_map.dll guncelleniyor (derleme klasoru)")
if(MAP_DEPLOY AND EXISTS "${XPLANE_DIR}/Resources/plugins")
    add_custom_command(TARGET plugin_api_test_dll POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/64"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:prime_map> "${DEPLOY_DIR}/64/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:plugin_api_test> "${DEPLOY_DIR}/64/")
endif()
