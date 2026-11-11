# prime_map_plugin: Prime Map API'yi kullanan resmi X-Plane eklentisi (ince kabuk).
# Haritanin motoru prime_map_api projesinde; bu proje sadece giris noktasini (PluginMain.cpp),
# ayar dosyalarini ve kurulumu icerir.
#
#   cmake -S <bu klasor> -B <derleme> -G Ninja -DCMAKE_BUILD_TYPE=Release
#   ninja -C <derleme>   ->  <derleme>/prime-map-plugin/64/win.xpl (+ prime_map.dll)
cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0091 NEW)  # MSVC runtime secimi icin
project(prime_map_plugin VERSION 0.22 LANGUAGES CXX)

# COTS: ucuncu taraf bileşenler repoda tutulmaz; C:\workspace\cots altindan okunur.
set(COTS_DIR "C:/workspace/cots" CACHE PATH "Ucuncu taraf bileşen klasoru (COTS)")
# X-Plane SDK: <COTS>/X-Plane-SDK (kurulum: README -> Ucuncu Taraf Bilesenler)
set(XPSDK "${COTS_DIR}/X-Plane-SDK" CACHE PATH "X-Plane SDK klasoru")
# Prime Map kutuphanesinin reposu
set(PRIME_MAP_DIR "" CACHE PATH "prime_map_api klasoru (PrimeMap.h + CMakeLists.txt)")
if(NOT PRIME_MAP_DIR)
    set(PRIME_MAP_DIR "${CMAKE_SOURCE_DIR}/../prime_map_api")
endif()
set(XPLANE_DIR "C:/X-Plane 11" CACHE PATH "X-Plane 11 klasoru")
option(MAP_DEPLOY "Derlemeden sonra plugini X-Plane'e kopyala" ON)

# Eski derleme klasorunde kalmis bayat yollari kendiliginden duzelt
if(NOT EXISTS "${XPSDK}/CHeaders/XPLM/XPLMDisplay.h" AND
   EXISTS "${COTS_DIR}/X-Plane-SDK/CHeaders/XPLM/XPLMDisplay.h")
    set(XPSDK "${COTS_DIR}/X-Plane-SDK" CACHE PATH "X-Plane SDK klasoru" FORCE)
endif()
if(NOT EXISTS "${PRIME_MAP_DIR}/PrimeMap.h" AND
   EXISTS "${CMAKE_SOURCE_DIR}/../prime_map_api/PrimeMap.h")
    set(PRIME_MAP_DIR "${CMAKE_SOURCE_DIR}/../prime_map_api" CACHE PATH "prime_map_api klasoru (PrimeMap.h + CMakeLists.txt)" FORCE)
endif()

if(NOT EXISTS "${XPSDK}/CHeaders/XPLM/XPLMDisplay.h")
    message(FATAL_ERROR "X-Plane SDK bulunamadi: ${XPSDK}\n"
                        "Kurulum: README.md -> Ucuncu Taraf Bilesenler")
endif()
if(NOT EXISTS "${PRIME_MAP_DIR}/PrimeMap.h")
    message(FATAL_ERROR "prime_map_api bulunamadi: ${PRIME_MAP_DIR}\n"
                        "prime_map_api reposu bu klasorun kardesi olmali: ${CMAKE_SOURCE_DIR}/../prime_map_api")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# CRT: /MD. prime_map ile ayni CRT zorunlu; STL tipleri DLL sinirindan geciyor.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# Kutuphaneyi ayni CMake oturumuna kat.
add_subdirectory(${PRIME_MAP_DIR} prime_map_build)

# Ince kabuk: X-Plane giris noktasi + ayar dosyasi yardimcisi.
# Butun harita islevi prime_map DLL'indedir (prime_map_api projesi).
# API'nin genel basliklari (PrimeMap.h, primemapglobal.h) bu listede YOK:
# prime_map_api alt projesi agacta kendinde gosterir; buraya almak Qt'te
# "<Other Locations>" gurultusu yaratirdi.
add_library(prime-map-plugin SHARED
    src/plugin/PluginMain.cpp
    src/Settings.cpp
    src/Settings.h)

target_include_directories(prime-map-plugin PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(prime-map-plugin SYSTEM PRIVATE ${XPSDK}/CHeaders/XPLM)
target_compile_definitions(prime-map-plugin PRIVATE XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1
                           IBM=1 APL=0 LIN=0 _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(prime-map-plugin PRIVATE /W4 /utf-8 /permissive-)

# Kutuphane hedef adina linklenir; include yolu transitif gelir:
#   #include <prime_map_api/PrimeMap.h>
target_link_libraries(prime-map-plugin PRIVATE
    prime_map
    ${XPSDK}/Libraries/Win/XPLM_64.lib
    opengl32)

# Windows surum kaynagi: surum bilgisi CMake PROJECT_VERSION'dan win.xpl'e gomulur
# (Dosya Ozellikleri -> Ayrintilar).
configure_file(${CMAKE_SOURCE_DIR}/prime_map_plugin.rc.in
               ${CMAKE_BINARY_DIR}/prime_map_plugin.rc @ONLY)
target_sources(prime-map-plugin PRIVATE ${CMAKE_BINARY_DIR}/prime_map_plugin.rc)

# Cikti: <derleme>/prime-map-plugin/64/win.xpl
set(OUT_DIR ${CMAKE_BINARY_DIR}/prime-map-plugin)
set_target_properties(prime-map-plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME win
    SUFFIX ".xpl"
    RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR}/64
    LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR}/64)

# prime_map.dll pluginin yanina kopyalanmali (win.xpl onu yukler)
add_custom_command(TARGET prime-map-plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:prime_map> "${OUT_DIR}/64/"
    COMMENT "prime_map.dll pluginin yanina kopyalaniyor")

# Derleme bitince X-Plane'e kur (X-Plane aciksa win.xpl kilitlidir, once kapat)
if(MAP_DEPLOY AND EXISTS "${XPLANE_DIR}/Resources/plugins")
    set(DEPLOY_DIR "${XPLANE_DIR}/Resources/plugins/prime-map-plugin")
    add_custom_command(TARGET prime-map-plugin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/64"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:prime-map-plugin> "${DEPLOY_DIR}/64/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:prime_map> "${DEPLOY_DIR}/64/"
        COMMENT "Plugin X-Plane'e kuruluyor: ${DEPLOY_DIR}")
endif()
