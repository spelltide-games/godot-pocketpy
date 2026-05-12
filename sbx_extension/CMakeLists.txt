cmake_minimum_required(VERSION 3.17)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

project(sbx_extension)

if(NOT MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffp-contract=off")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffp-contract=off")
endif()

include_directories(SYSTEM ${CMAKE_CURRENT_LIST_DIR}/system)

option(LEVELDB_BUILD_TESTS "Build LevelDB's unit tests" OFF)
option(LEVELDB_BUILD_BENCHMARKS "Build LevelDB's benchmarks" OFF)
option(LEVELDB_INSTALL "Install LevelDB's header and library" OFF)
set(HAVE_CRC32C "" CACHE INTERNAL OFF)
set(HAVE_SNAPPY "" CACHE INTERNAL OFF)
set(HAVE_ZSTD "" CACHE INTERNAL OFF)
set(HAVE_TCMALLOC "" CACHE INTERNAL OFF)
add_subdirectory(leveldb)
target_compile_definitions(leveldb PUBLIC -DHAVE_SNAPPY=1)
target_link_libraries(leveldb lz4)

file(GLOB_RECURSE SBX_SRC ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)

add_library(${PROJECT_NAME} STATIC
    ${SBX_SRC}
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    leveldb
    godot::cpp
    pocketpy
)

