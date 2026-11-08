# Prime Map API test eklentisi: AYRI bir proje. Sadece SDK paketini kullanir (include/PrimeMap.h + lib).
#
#   cmake -S examples/map_api_test -B <derleme> -G Ninja -DCMAKE_BUILD_TYPE=Release ^
#         -DPRIME_MAP_SDK=<prime_map_sdk klasoru> -DXPSDK=<X-Plane SDK klasoru>
#   ninja -C <derleme>   ->  <derleme>/map_api_test/64/win.xpl
cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0091 NEW)
project(map_api_test LANGUAGES CXX)

set(PRIME_MAP_SDK "" CACHE PATH "prime_map_sdk klasoru (include/, lib/)")
set(XPSDK "" CACHE PATH "X-Plane SDK klasoru")
option(PRIME_MAP_DYNAMIC_CRT "/MD ile derle (prime_map_MD.lib)" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(PRIME_MAP_DYNAMIC_CRT)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    set(CRT MD)
else()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(CRT MT)
endif()

add_library(map_api_test SHARED src/TestPluginMain.cpp src/MapApiCalls.cpp src/SelfTest.cpp)
target_include_directories(map_api_test PRIVATE ${PRIME_MAP_SDK}/include)
target_include_directories(map_api_test SYSTEM PRIVATE ${XPSDK}/CHeaders/XPLM)
target_compile_definitions(map_api_test PRIVATE XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1 IBM=1 APL=0 LIN=0
                           _CRT_SECURE_NO_WARNINGS)
target_compile_options(map_api_test PRIVATE /W4 /utf-8 /permissive-)
target_link_libraries(map_api_test PRIVATE
    ${PRIME_MAP_SDK}/lib/prime_map_${CRT}$<$<CONFIG:Debug>:d>.lib
    ${XPSDK}/Libraries/Win/XPLM_64.lib
    opengl32)
set_target_properties(map_api_test PROPERTIES PREFIX "" OUTPUT_NAME win SUFFIX ".xpl"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/map_api_test/64
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/map_api_test/64)
