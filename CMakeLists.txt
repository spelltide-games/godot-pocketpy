# Using the same minimum as the godot-cpp project
cmake_minimum_required(VERSION 3.17)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Build pocketpy
option(PK_ENABLE_CUSTOM_SNAME "" ON)
option(PK_ENABLE_DLL "" OFF)
option(PK_BUILD_STATIC_LIB "" ON)
option(PK_BUILD_MODULE_LZ4 "" ON)
option(PK_BUILD_MODULE_CUTE_PNG "" ON)
option(PK_BUILD_MODULE_MSGPACK "" ON)
add_subdirectory(pocketpy)

##############################################
# Silence unused variable warning when specified from toolchain
if(CMAKE_C_COMPILER)
endif()

set(LIBNAME "godot-pocketpy" CACHE STRING "The name of the library")
set(GODOT_PROJECT_DIR "demo" CACHE STRING "The directory of a Godot project folder")

set(GODOTCPP_USE_STATIC_CPP OFF CACHE BOOL "")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(GODOTCPP_DEBUG_CRT ON CACHE BOOL "")
endif()

find_package(Python3 3.10 REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(godot-cpp SYSTEM)

# Add godot-cpp's module path and include the exported functions.
# This is made available for documentation generation
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${godot-cpp_SOURCE_DIR}/cmake")
include(GodotCPPModule)

# The godot-cpp target has some of useful properties attached that can be retrieved like so.
get_target_property(GODOTCPP_SUFFIX godot::cpp GODOTCPP_SUFFIX)
get_target_property(GODOTCPP_PLATFORM godot::cpp GODOTCPP_PLATFORM)

# Now we can specify our own project which will inherit any global cmake properties or variables that have been defined.
project(godot-pocketpy
    VERSION 1.0
    DESCRIPTION "Python scripting extension for Godot 4.0+ based on pocketpy."
    HOMEPAGE_URL "https://github.com/pocketpy/godot-pocketpy"
    LANGUAGES CXX
)

add_library(${LIBNAME} SHARED)

file(GLOB_RECURSE LANG_SRC ${CMAKE_CURRENT_LIST_DIR}/src/lang/*.cpp)
file(GLOB_RECURSE SUPPORT_SRC ${CMAKE_CURRENT_LIST_DIR}/src/support/*.cpp)

target_sources(${LIBNAME}
    PRIVATE
    src/register_types.cpp
    src/register_types.h
    ${LANG_SRC}
    ${SUPPORT_SRC}
)

if(NOT MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffp-contract=off")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffp-contract=off")
endif()

add_subdirectory(sbx_extension)

target_link_libraries(${LIBNAME} PRIVATE
    godot-cpp
    pocketpy
    sbx_extension
)

set(GODOT_PROJECT_BINARY_DIR "${PROJECT_SOURCE_DIR}/${GODOT_PROJECT_DIR}/addons/godot-pocketpy/bin/${GODOTCPP_PLATFORM}")

set_target_properties(${LIBNAME}
    PROPERTIES
    # The generator expression here prevents msvc from adding a Debug or Release subdir.
    RUNTIME_OUTPUT_DIRECTORY "$<1:${GODOT_PROJECT_BINARY_DIR}>"
    
    OUTPUT_NAME "${LIBNAME}${GODOTCPP_SUFFIX}"
)

add_custom_command(TARGET ${LIBNAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GODOT_PROJECT_BINARY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E echo "Copying $<TARGET_FILE_NAME:${LIBNAME}> to ${GODOT_PROJECT_BINARY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${LIBNAME}>"
            "${GODOT_PROJECT_BINARY_DIR}/$<TARGET_FILE_NAME:${LIBNAME}>"
    COMMENT "POST_BUILD begin for ${LIBNAME}"
)
