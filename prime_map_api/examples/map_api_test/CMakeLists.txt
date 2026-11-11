# Prime Map API test eklentisi: AYRI bir proje. Kutuphaneyi add_subdirectory ile
# ayni oturuma katar, hedef adina (prime_map) linkler.
#
#   cmake -S examples/map_api_test -B <derleme> -G Ninja -DCMAKE_BUILD_TYPE=Release ^
#         -DXPSDK=<X-Plane SDK klasoru>
#   ninja -C <derleme>   ->  <derleme>/map_api_test/64/win.xpl (+ prime_map.dll)
cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0091 NEW)
project(map_api_test LANGUAGES CXX)

set(COTS_DIR "C:/workspace/cots" CACHE PATH "Ucuncu taraf bileşen klasoru (COTS)")
set(XPSDK "${COTS_DIR}/X-Plane-SDK" CACHE PATH "X-Plane SDK klasoru")
# Kutuphane reposu: bu klasorun iki ustu (prime_map_api/examples/map_api_test)
set(PRIME_MAP_DIR "" CACHE PATH "prime_map_api klasoru (PrimeMap.h + CMakeLists.txt)")
if(NOT PRIME_MAP_DIR)
    set(PRIME_MAP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
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

add_library(map_api_test SHARED
    src/TestPluginMain.cpp
    src/MapApiCalls.h
    src/MapApiCalls.cpp
    src/SelfTest.h
    src/SelfTest.cpp)
target_include_directories(map_api_test SYSTEM PRIVATE ${XPSDK}/CHeaders/XPLM)
target_compile_definitions(map_api_test PRIVATE XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1 IBM=1 APL=0 LIN=0
                           _CRT_SECURE_NO_WARNINGS)
target_compile_options(map_api_test PRIVATE /W4 /utf-8 /permissive-)
target_link_libraries(map_api_test PRIVATE
    prime_map
    ${XPSDK}/Libraries/Win/XPLM_64.lib
    opengl32)
set_target_properties(map_api_test PROPERTIES PREFIX "" OUTPUT_NAME win SUFFIX ".xpl"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/map_api_test/64
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/map_api_test/64)

# prime_map.dll pluginin yanina kopyalanmali
add_custom_command(TARGET map_api_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:prime_map> ${CMAKE_BINARY_DIR}/map_api_test/64/)
