# plugin_api_test: Prime Map API'yi denemek icin AYRI bir X-Plane eklentisi.
# Su an (adim 1) sadece kendi UI'i var; API hic baglanmadi.
#
#   cmake -S C:\workspace\projects\plugin-api-test -B <derleme> -G Ninja -DCMAKE_BUILD_TYPE=Release
#   ninja -C <derleme>   ->  <derleme>/plugin_api_test/64/win.xpl
#
# Sonraki adimda PRIME_MAP_SDK (prime_map_sdk klasoru) eklenince kutuphane linklenecek.
cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0091 NEW)
project(plugin_api_test LANGUAGES CXX)

# COTS: ucuncu taraf bile┼ƒenler repoda tutulmaz; C:\workspace\cots altindan okunur.
set(COTS_DIR "C:/workspace/cots" CACHE PATH "Ucuncu taraf bile┼ƒen klasoru (COTS)")
set(XPSDK "${COTS_DIR}/X-Plane-SDK" CACHE PATH "X-Plane SDK klasoru")

# SDK_BOUND: Prime Map SDK paketi (include/PrimeMap.h + lib/prime_map_MT.lib).
# Uretim: cmake --install <ana proje derlemesi> --prefix C:\workspace\projects\prime_map_sdk
set(PRIME_MAP_SDK "" CACHE PATH "prime_map_sdk klasoru (include/, lib/)")
if(NOT PRIME_MAP_SDK)
    # Varsayilan: proje yanindaki SDK paketi
    set(PRIME_MAP_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../prime_map_sdk")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Ana kutuphane /MT ile derleniyor; ayni CRT ile kalmak icin statik CRT (sonraki adimda da ayni)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# SDK_BOUND: sarmalayici (MapApiCalls) derlemeye girdi; haritaya erisim sadece bu siniftan gecer
add_library(plugin_api_test SHARED src/MapUiTestMain.cpp src/MapApiCalls.cpp)

# SDK_BOUND: SDK paketinin basligi (PrimeMap.h) include yolu
target_include_directories(plugin_api_test PRIVATE ${PRIME_MAP_SDK}/include)
target_include_directories(plugin_api_test SYSTEM PRIVATE ${XPSDK}/CHeaders/XPLM)
target_compile_definitions(plugin_api_test PRIVATE XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1
                       IBM=1 APL=0 LIN=0 _CRT_SECURE_NO_WARNINGS)
target_compile_options(plugin_api_test PRIVATE /W4 /utf-8 /permissive-)
# SDK_BOUND: prime_map_MT.lib (statik kutuphane; /MT, ana projeyle ayni CRT) linkleniyor
target_link_libraries(plugin_api_test PRIVATE
    ${PRIME_MAP_SDK}/lib/prime_map_MT.lib
    ${XPSDK}/Libraries/Win/XPLM_64.lib
    opengl32)
set_target_properties(plugin_api_test PROPERTIES PREFIX "" OUTPUT_NAME win SUFFIX ".xpl"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugin_api_test/64)