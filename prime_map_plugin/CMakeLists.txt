# prime_map_plugin: Prime Map API'yi kullanan resmi X-Plane eklentisi (ince kabuk).
# Haritanin motoru prime_map_api projesinde; bu proje sadece giris noktasini (PluginMain.cpp),
# ayar dosyalarini ve kurulumu icerir. SDK paketinden beslenir:
#
#   prime_map_api projesinden SDK uretim:
#     cmake --install <prime_map_api derlemesi> --prefix <klasor>/prime_map_sdk
#
#   Bu projenin derlemesi:
#     cmake -S <bu klasor> -B <derleme> -G Ninja -DCMAKE_BUILD_TYPE=Release
#     ninja -C <derleme>   ->  <derleme>/prime-map-plugin/64/win.xpl
cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0091 NEW)  # MSVC runtime secimi icin
project(prime_map_plugin VERSION 0.20 LANGUAGES CXX)

# COTS: ucuncu taraf bile┼ƒenler repoda tutulmaz; C:\workspace\cots altindan okunur.
set(COTS_DIR "C:/workspace/cots" CACHE PATH "Ucuncu taraf bile┼ƒen klasoru (COTS)")
# X-Plane SDK: <COTS>/X-Plane-SDK (kurulum: README -> Ucuncu Taraf Bilesenler)
set(XPSDK "${COTS_DIR}/X-Plane-SDK" CACHE PATH "X-Plane SDK klasoru")
# Prime Map SDK paketi (include/PrimeMap.h + lib/)
set(PRIME_MAP_SDK "" CACHE PATH "prime_map_sdk klasoru (include/, lib/)")
if(NOT PRIME_MAP_SDK)
    set(PRIME_MAP_SDK "${CMAKE_SOURCE_DIR}/../prime_map_sdk")
endif()
set(XPLANE_DIR "C:/X-Plane 11" CACHE PATH "X-Plane 11 klasoru")
option(MAP_DEPLOY "Derlemeden sonra plugini X-Plane'e kopyala" ON)
# OFF = /MT (prime_map_MT.lib), ON = /MD (prime_map_MD.lib);
# prime_map_api hangi CRT ile derlendiyse ayni olmali.
option(PRIME_MAP_DYNAMIC_CRT "prime_map'i /MD ile linkle (OFF: /MT)" OFF)

# Eski derleme klasorunde kalmis bayat yollari kendiliginden duzelt
if(NOT EXISTS "${XPSDK}/CHeaders/XPLM/XPLMDisplay.h" AND
   EXISTS "${COTS_DIR}/X-Plane-SDK/CHeaders/XPLM/XPLMDisplay.h")
    set(XPSDK "${COTS_DIR}/X-Plane-SDK" CACHE PATH "X-Plane SDK klasoru" FORCE)
endif()
if(NOT EXISTS "${PRIME_MAP_SDK}/include/PrimeMap.h" AND
   EXISTS "${CMAKE_SOURCE_DIR}/../prime_map_sdk/include/PrimeMap.h")
    set(PRIME_MAP_SDK "${CMAKE_SOURCE_DIR}/../prime_map_sdk" CACHE PATH "prime_map_sdk klasoru (include/, lib/)" FORCE)
endif()

if(NOT EXISTS "${XPSDK}/CHeaders/XPLM/XPLMDisplay.h")
    message(FATAL_ERROR "X-Plane SDK bulunamadi: ${XPSDK}\n"
                        "Kurulum: README.md -> Ucuncu Taraf Bilesenler")
endif()
if(NOT EXISTS "${PRIME_MAP_SDK}/include/PrimeMap.h")
    message(FATAL_ERROR "Prime Map SDK bulunamadi: ${PRIME_MAP_SDK}\n"
                        "prime_map_api projesinden uretin: cmake --install <derleme> --prefix <klasor>")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(PRIME_MAP_DYNAMIC_CRT)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    set(PRIME_MAP_CRT MD)
else()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(PRIME_MAP_CRT MT)
endif()

# Ince kabuk: X-Plane giris noktasi + ayar dosyasi yardimcisi.
# Butun harita islevi prime_map_MT.lib icindedir (SDK paketi).
add_library(prime-map-plugin SHARED
    src/plugin/PluginMain.cpp
    src/Settings.cpp
    src/Settings.h
    ${PRIME_MAP_SDK}/include/PrimeMap.h)

target_include_directories(prime-map-plugin PRIVATE ${PRIME_MAP_SDK}/include)
target_include_directories(prime-map-plugin PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(prime-map-plugin SYSTEM PRIVATE ${XPSDK}/CHeaders/XPLM)
target_compile_definitions(prime-map-plugin PRIVATE XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1
                           IBM=1 APL=0 LIN=0 _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(prime-map-plugin PRIVATE /W4 /utf-8 /permissive-)

target_link_libraries(prime-map-plugin PRIVATE
    ${PRIME_MAP_SDK}/lib/prime_map_${PRIME_MAP_CRT}$<$<CONFIG:Debug>:d>.lib
    ${XPSDK}/Libraries/Win/XPLM_64.lib
    opengl32)

# Cikti: <derleme>/prime-map-plugin/64/win.xpl
set(OUT_DIR ${CMAKE_BINARY_DIR}/prime-map-plugin)
set_target_properties(prime-map-plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME win
    SUFFIX ".xpl"
    RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR}/64
    LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR}/64)

# Derleme bitince X-Plane'e kur (X-Plane aciksa win.xpl kilitlidir, once kapat)
if(MAP_DEPLOY AND EXISTS "${XPLANE_DIR}/Resources/plugins")
    set(DEPLOY_DIR "${XPLANE_DIR}/Resources/plugins/prime-map-plugin")
    add_custom_command(TARGET prime-map-plugin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/64"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:prime-map-plugin> "${DEPLOY_DIR}/64/"
        COMMENT "Plugin X-Plane'e kuruluyor: ${DEPLOY_DIR}")
endif()