cmake_minimum_required(VERSION 3.11)
project(FlexASIO DESCRIPTION "FlexASIO Universal ASIO Driver")

if(DEFINED ENV{VCPKG_DIR})
	include("$ENV{VCPKG_DIR}/scripts/buildsystems/vcpkg.cmake")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set(FLEXASIO_PLATFORM x86)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(FLEXASIO_PLATFORM x64)
else()
	set(FLEXASIO_PLATFORM unknown)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMakeModules")
find_package(portaudio REQUIRED)
find_package(tinytoml REQUIRED)

# Put EXEs and DLLs in the same directory so that EXEs can find their dependencies.
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(CMAKE_CXX_STANDARD 17)

add_definitions(-DBUILD_CONFIGURATION="$<CONFIG>" -DBUILD_PLATFORM="${FLEXASIO_PLATFORM}")

add_subdirectory(ASIOHost)
add_subdirectory(FlexASIOUtil EXCLUDE_FROM_ALL)
add_subdirectory(FlexASIO)
add_subdirectory(FlexASIOTest)
add_subdirectory(PortAudioDevices)
