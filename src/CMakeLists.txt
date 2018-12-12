cmake_minimum_required(VERSION 3.11)
project(FlexASIO DESCRIPTION "FlexASIO Universal ASIO Driver")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMakeModules")
find_package(portaudio REQUIRED)

set(CMAKE_CXX_STANDARD 17)

# TODO: fix the platform. Might be easier to do this using the preprocessor, as CMake doesn't seem to provide a variable for the architecture.
add_definitions(-DBUILD_CONFIGURATION="$<CONFIG>" -DBUILD_PLATFORM="unknown")

add_subdirectory(ASIOHost)
add_subdirectory(FlexASIOUtil EXCLUDE_FROM_ALL)
add_subdirectory(FlexASIO)
add_subdirectory(FlexASIOTest)
add_subdirectory(PortAudioDevices)
