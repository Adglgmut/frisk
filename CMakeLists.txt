cmake_minimum_required(VERSION 2.6)

project(frisk)

if(UNIX)
    set(CMAKE_CXX_FLAGS "-g")
    set(CMAKE_C_FLAGS "-g")
endif()

if(MSVC)
    set(CMAKE_C_FLAGS /wd4996)
endif()

if(APPLE)
    add_definitions(-DFRISK_PLATFORM_OSX=1)
else()
    if(UNIX)
        add_definitions(-DFRISK_PLATFORM_LINUX=1)
    endif()

    if(MSVC)
        add_definitions(-DFRISK_PLATFORM_WIN32=1)
    endif()
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/dynamic/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)

add_subdirectory(external)
add_subdirectory(lib)
add_subdirectory(apps)

