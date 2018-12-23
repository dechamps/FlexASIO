cmake_minimum_required(VERSION 3.11)
project(FlexASIO DESCRIPTION "FlexASIO Universal ASIO Driver")

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set(FLEXASIO_PLATFORM x86)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(FLEXASIO_PLATFORM x64)
else()
	set(FLEXASIO_PLATFORM unknown)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMakeModules")
find_package(tinytoml REQUIRED)
find_package(portaudio REQUIRED)
find_package(Git)

set(CMAKE_CXX_STANDARD 17)
add_compile_options(
	/WX /W4 /permissive- /analyze

	# Suppress warnings about shadowing declarations.
	#
	# In most cases, this happens when a lambda is used to initialize some
	# variable, and the lambda declares a local variable with the same name as the
	# variable it's tasked with initializing. In such cases the shadowing is
	# actually desirable, because it prevents one from accidentally using the (not
	# yet initialized) outer variable instead of the (valid) local variable within
	# the lambda.
	/wd4458 /wd4456
)
add_definitions(
	-DBUILD_CONFIGURATION="$<CONFIG>"
	-DBUILD_PLATFORM="${FLEXASIO_PLATFORM}"
)

add_subdirectory(asiosdk EXCLUDE_FROM_ALL)
add_subdirectory(FlexASIOUtil EXCLUDE_FROM_ALL)
add_subdirectory(FlexASIO)
add_subdirectory(FlexASIOTest)
add_subdirectory(PortAudioDevices)
add_subdirectory(endian)
add_subdirectory(version)

