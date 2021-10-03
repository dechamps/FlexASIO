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
find_package(tinytoml MODULE REQUIRED)
find_package(PortAudio CONFIG REQUIRED)
find_package(dechamps_cpplog CONFIG REQUIRED)
find_package(dechamps_cpputil CONFIG REQUIRED)
find_package(dechamps_ASIOUtil CONFIG REQUIRED)
find_package(ASIOTest CONFIG REQUIRED)

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

add_subdirectory(../dechamps_CMakeUtils/version version EXCLUDE_FROM_ALL)

add_subdirectory(FlexASIOUtil EXCLUDE_FROM_ALL)
add_subdirectory(FlexASIO)
add_subdirectory(FlexASIOTest)
add_subdirectory(PortAudioDevices)
