cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0091 NEW)
project(plugin-api-test LANGUAGES CXX)

set(COTS_DIR "C:/workspace/cots" CACHE PATH "Ucuncu taraf bilesen klasoru (COTS)")
if(NOT EXISTS "${COTS_DIR}/X-Plane-SDK" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../cots/X-Plane-SDK")
    get_filename_component(_pm_cots "${CMAKE_CURRENT_SOURCE_DIR}/../cots" ABSOLUTE)
    set(COTS_DIR "${_pm_cots}" CACHE PATH "Ucuncu taraf bilesen klasoru (COTS)" FORCE)
    unset(_pm_cots)
endif()
set(XPSDK "${COTS_DIR}/X-Plane-SDK" CACHE PATH "X-Plane SDK klasoru")
set(PRIME_MAP_DIR "" CACHE PATH "prime_map_api klasoru (PrimeMap.h + CMakeLists.txt)")
if(NOT PRIME_MAP_DIR)
    set(PRIME_MAP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../prime_map_api")
endif()

if(NOT EXISTS "${XPSDK}/CHeaders/XPLM/XPLMDisplay.h" AND EXISTS "${COTS_DIR}/X-Plane-SDK/CHeaders/XPLM/XPLMDisplay.h")
    set(XPSDK "${COTS_DIR}/X-Plane-SDK" CACHE PATH "X-Plane SDK klasoru" FORCE)
endif()
if(NOT EXISTS "${PRIME_MAP_DIR}/PrimeMap.h")
    message(FATAL_ERROR "prime_map_api bulunamadi: ${PRIME_MAP_DIR}")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

add_subdirectory(${PRIME_MAP_DIR} prime_map_build)

add_library(${PROJECT_NAME} SHARED
    src/XMapUiTestMain.cpp
    src/XMapApiCalls.h src/XMapApiCalls.cpp
    src/MultiXMapSelfTest.h src/MultiXMapSelfTest.cpp)

target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${XPSDK}/CHeaders/XPLM)
target_compile_definitions(${PROJECT_NAME} PRIVATE XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1
                           IBM=1 APL=0 LIN=0 _CRT_SECURE_NO_WARNINGS)
target_compile_options(${PROJECT_NAME} PRIVATE /W4 /wd4251 /utf-8 /permissive-)
target_link_libraries(${PROJECT_NAME} PRIVATE
    prime-map-api
    ${XPSDK}/Libraries/Win/XPLM_64.lib
    opengl32)
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" OUTPUT_NAME win SUFFIX ".xpl"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugin_api_test/64)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:prime-map-api> ${CMAKE_BINARY_DIR}/plugin_api_test/64/)

set(XPLANE_DIR "C:/X-Plane 11" CACHE PATH "X-Plane 11 klasoru")
option(MAP_DEPLOY "Derlemeden sonra test eklentisini X-Plane'e kopyala" OFF)
if(MAP_DEPLOY AND EXISTS "${XPLANE_DIR}/Resources/plugins")
    set(DEPLOY_DIR "${XPLANE_DIR}/Resources/plugins/plugin-api-test")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/64"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${PROJECT_NAME}> "${DEPLOY_DIR}/64/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:prime-map-api> "${DEPLOY_DIR}/64/"
        COMMENT "Test eklentisi X-Plane'e kuruluyor: ${DEPLOY_DIR}")
endif()

add_custom_target(plugin_api_test_dll ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:prime-map-api> ${CMAKE_BINARY_DIR}/plugin_api_test/64/
    DEPENDS prime-map-api ${PROJECT_NAME}
    COMMENT "prime_map.dll guncelleniyor (derleme klasoru)")
if(MAP_DEPLOY AND EXISTS "${XPLANE_DIR}/Resources/plugins")
    add_custom_command(TARGET plugin_api_test_dll POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/64"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:prime-map-api> "${DEPLOY_DIR}/64/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${PROJECT_NAME}> "${DEPLOY_DIR}/64/")
endif()
