# FlexASIO CMake superbuild.

cmake_minimum_required(VERSION 3.11)
project(flexasio DESCRIPTION "FlexASIO Universal ASIO Driver Superbuild")
include(ExternalProject)
set(INTERNAL_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/install")
set(CMAKE_ARGS
    "-DCMAKE_INSTALL_PREFIX=${INTERNAL_INSTALL_PREFIX}"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
)

function(check_git_submodule DIR)
    file(GLOB DIR_FILES "${DIR}/*")
	if (DIR_FILES STREQUAL "")
		message(WARNING "It looks like the '${DIR}' directory is empty. Did you forget to update git submodules?")
		find_package(Git)
		if (Git_FOUND)
			message(STATUS "Updating the '${DIR}' git submodule to fix.")
			execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_CURRENT_LIST_DIR}" submodule update --init -- "${DIR}")
		endif()
	endif()
endfunction()

check_git_submodule(tinytoml)
ExternalProject_Add(
    tinytoml
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/tinytoml"
    INSTALL_DIR "${INTERNAL_INSTALL_PREFIX}"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND "" BUILD_ALWAYS
    INSTALL_COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_LIST_DIR}/tinytoml/include" "${INTERNAL_INSTALL_PREFIX}/include"
    EXCLUDE_FROM_ALL
)

check_git_submodule(portaudio)
ExternalProject_Add(
	portaudio
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/portaudio"
	INSTALL_DIR "${INTERNAL_INSTALL_PREFIX}"
    BUILD_ALWAYS TRUE USES_TERMINAL_BUILD TRUE
	CMAKE_ARGS ${CMAKE_ARGS}
		-DPA_ENABLE_DEBUG_OUTPUT=ON
		"-DCMAKE_PROJECT_portaudio_INCLUDE=${CMAKE_CURRENT_LIST_DIR}/portaudio.cmake"
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
