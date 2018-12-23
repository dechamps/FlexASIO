# FlexASIO CMake superbuild.

cmake_minimum_required(VERSION 3.11)
project(flexasio DESCRIPTION "FlexASIO Universal ASIO Driver Superbuild")
include(ExternalProject)
set(INTERNAL_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/install")
set(CMAKE_ARGS
    "-DCMAKE_INSTALL_PREFIX=${INTERNAL_INSTALL_PREFIX}"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
)

ExternalProject_Add(
    tinytoml
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/tinytoml"
    INSTALL_DIR "${INTERNAL_INSTALL_PREFIX}"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND "" BUILD_ALWAYS
    INSTALL_COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_LIST_DIR}/tinytoml/include" "${INTERNAL_INSTALL_PREFIX}/include"
    EXCLUDE_FROM_ALL
)

find_path(DXSDK_INCLUDE_DIR dsound.h)
find_library(DXSDK_DSOUND_LIBRARY dsound.lib)
ExternalProject_Add(
	portaudio
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/portaudio"
	INSTALL_DIR "${INTERNAL_INSTALL_PREFIX}"
    BUILD_ALWAYS TRUE USES_TERMINAL_BUILD TRUE
	# In the version we're currently using, PortAudio doesn't have an install target.
	CMAKE_ARGS ${CMAKE_ARGS}
		"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=${INTERNAL_INSTALL_PREFIX}/bin"
		"-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${INTERNAL_INSTALL_PREFIX}/bin"
		"-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=${INTERNAL_INSTALL_PREFIX}/lib"
		-DPA_ENABLE_DEBUG_OUTPUT=ON
		"-DCMAKE_C_FLAGS=-DPA_GIT_REVISION=396fe4b6699ae929d3a685b3ef8a7e97396139a4-FlexASIO"  # TODO: stop hardcoding this
		# Work around https://lists.columbia.edu/pipermail/portaudio/2017-January/001001.html
		-DMSVS=ON
		# Work around lack of https://app.assembla.com/spaces/portaudio/git/commits/0baa1e2e31f37bac2883ea59e50b76037271426d
		"-DDXSDK_ROOT_DIR=${DXSDK_INCLUDE_DIR}/.." "-DDXSDK_INCLUDE_DIR=${DXSDK_INCLUDE_DIR}" "-DDXSDK_DSOUND_LIBRARY=${DXSDK_DSOUND_LIBRARY}"
		INSTALL_COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_LIST_DIR}/portaudio/include" "${INTERNAL_INSTALL_PREFIX}/include"
	EXCLUDE_FROM_ALL
)

ExternalProject_Add(
    FlexASIO
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/flexasio"
    USES_TERMINAL_CONFIGURE TRUE
    BUILD_ALWAYS TRUE USES_TERMINAL_BUILD TRUE
    INSTALL_DIR "${INTERNAL_INSTALL_PREFIX}"
    CMAKE_ARGS ${CMAKE_ARGS}
    DEPENDS tinytoml portaudio
)

install(DIRECTORY "${INTERNAL_INSTALL_PREFIX}/" DESTINATION "${CMAKE_INSTALL_PREFIX}")
include(InstallRequiredSystemLibraries)
