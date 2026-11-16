# Prime XMap API - en kucuk host eklentisi (WindowStyle::Texture).
# Kendi eklentinizde bu dosyayi kopyalayin; degisen tek sey PRIME_MAP_DIR'in varsayilani olur
# (kendi projenizde genellikle "${CMAKE_CURRENT_SOURCE_DIR}/../prime_map_api").
cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0091 NEW)                      # MSVC runtime secimi (CMAKE_MSVC_RUNTIME_LIBRARY)
project(texture-host VERSION 1.0 LANGUAGES CXX)

# --- Klasorler ---------------------------------------------------------------
set(PRIME_MAP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "prime_map_api klasoru (PrimeMap.h burada)")
set(COTS_DIR "C:/workspace/cots" CACHE PATH "Ucuncu taraf bilesenler (X-Plane-SDK, lzma, earcut, stb)")
if(NOT EXISTS "${COTS_DIR}/X-Plane-SDK")
    # Yoksa prime_map_api'nin kardesi "cots" klasoru
    get_filename_component(_th_cots "${PRIME_MAP_DIR}/../cots" ABSOLUTE)
    if(EXISTS "${_th_cots}/X-Plane-SDK")
        set(COTS_DIR "${_th_cots}" CACHE PATH "Ucuncu taraf bilesenler" FORCE)
    endif()
    unset(_th_cots)
endif()
set(XPSDK "${COTS_DIR}/X-Plane-SDK" CACHE PATH "X-Plane SDK klasoru")
set(XPLANE_DIR "C:/X-Plane 11" CACHE PATH "X-Plane 11 klasoru")
option(MAP_DEPLOY "Derlemeden sonra X-Plane'e kopyala" OFF)

if(NOT EXISTS "${PRIME_MAP_DIR}/PrimeMap.h")
    message(FATAL_ERROR "prime_map_api bulunamadi: ${PRIME_MAP_DIR} (-DPRIME_MAP_DIR=...)")
endif()
if(NOT EXISTS "${XPSDK}/CHeaders/XPLM/XPLMDisplay.h")
    message(FATAL_ERROR "X-Plane SDK bulunamadi: ${XPSDK} (-DCOTS_DIR=... ya da -DXPSDK=...)")
endif()

# --- Derleyici: kutuphane ile AYNI CRT (/MD) ----------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# --- Kutuphane: ayni CMake oturumunda derlenir (hedef adi: prime-map-api) -------
add_subdirectory(${PRIME_MAP_DIR} prime_map_build)

# --- Eklenti: <eklenti>/64/win.xpl --------------------------------------------
add_library(${PROJECT_NAME} SHARED src/TextureHost.cpp)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${XPSDK}/CHeaders/XPLM)
target_compile_definitions(${PROJECT_NAME} PRIVATE XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1 IBM=1 APL=0 LIN=0
                           _CRT_SECURE_NO_WARNINGS)
target_compile_options(${PROJECT_NAME} PRIVATE /W4 /utf-8 /permissive-)
target_link_libraries(${PROJECT_NAME} PRIVATE prime-map-api ${XPSDK}/Libraries/Win/XPLM_64.lib opengl32)

set(OUT_DIR ${CMAKE_BINARY_DIR}/texture-host)
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" OUTPUT_NAME win SUFFIX ".xpl"
    RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR}/64 LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR}/64)

# prime_map.dll her derlemede win.xpl'nin yanina (kutuphane degisince de: ALL hedefi)
add_custom_target(texture_host_dll ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:prime-map-api> "${OUT_DIR}/64/"
    DEPENDS prime-map-api ${PROJECT_NAME})

if(MAP_DEPLOY AND EXISTS "${XPLANE_DIR}/Resources/plugins")
    set(DEPLOY_DIR "${XPLANE_DIR}/Resources/plugins/texture-host")
    add_custom_command(TARGET texture_host_dll POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/64"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${PROJECT_NAME}> "${DEPLOY_DIR}/64/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:prime-map-api> "${DEPLOY_DIR}/64/"
        COMMENT "texture-host X-Plane'e kuruluyor: ${DEPLOY_DIR}")
endif()
