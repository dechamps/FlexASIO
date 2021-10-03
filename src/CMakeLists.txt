# FlexASIO CMake superbuild.

cmake_minimum_required(VERSION 3.11)
project(flexasio DESCRIPTION "FlexASIO Universal ASIO Driver Superbuild")
include(ExternalProject)
set(INTERNAL_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/install")
set(CMAKE_ARGS
    "-DCMAKE_INSTALL_PREFIX=${INTERNAL_INSTALL_PREFIX}"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
)

include(check_git_submodule.cmake)

check_git_submodule(dechamps_CMakeUtils)

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

check_git_submodule(cxxopts)
ExternalProject_Add(
    cxxopts
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/cxxopts"
    INSTALL_DIR "${INTERNAL_INSTALL_PREFIX}"
	CMAKE_ARGS ${CMAKE_ARGS}
    BUILD_ALWAYS
    EXCLUDE_FROM_ALL
)

check_git_submodule(libsndfile)
ExternalProject_Add(
    libsndfile
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/libsndfile"
    INSTALL_DIR "${INTERNAL_INSTALL_PREFIX}"
	CMAKE_ARGS ${CMAKE_ARGS} -DBUILD_PROGRAMS=OFF -DBUILD_SHARED_LIBS=ON
    BUILD_ALWAYS USES_TERMINAL_BUILD TRUE
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
		"-DCMAKE_PROJECT_PortAudio_INCLUDE=${CMAKE_CURRENT_LIST_DIR}/portaudio.cmake"
        # The CMAKE_INSTALL_INCLUDEDIR built-in was introduced in CMake 3.14.
        # Sadly current Visual Studio (16.11.3) ships with CMake 3.13, so we have to work around the missing built-in.
        -DCMAKE_INSTALL_INCLUDEDIR=include
	EXCLUDE_FROM_ALL
)

check_git_submodule(dechamps_cpputil)
ExternalProject_Add(
    dechamps_cpputil
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/dechamps_cpputil"
    BUILD_ALWAYS TRUE USES_TERMINAL_BUILD TRUE
    INSTALL_DIR "${INTERNAL_INSTALL_PREFIX}"
    CMAKE_ARGS ${CMAKE_ARGS}
	EXCLUDE_FROM_ALL
)

check_git_submodule(dechamps_cpplog)
ExternalProject_Add(
    dechamps_cpplog
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/dechamps_cpplog"
    BUILD_ALWAYS TRUE USES_TERMINAL_BUILD TRUE
    INSTALL_DIR "${INTERNAL_INSTALL_PREFIX}"
    CMAKE_ARGS ${CMAKE_ARGS}
	EXCLUDE_FROM_ALL
)

check_git_submodule(dechamps_ASIOUtil)
ExternalProject_Add(
    dechamps_ASIOUtil
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/dechamps_ASIOUtil"
	USES_TERMINAL_CONFIGURE TRUE
    BUILD_ALWAYS TRUE USES_TERMINAL_BUILD TRUE
    INSTALL_DIR "${INTERNAL_INSTALL_PREFIX}"
    CMAKE_ARGS ${CMAKE_ARGS}
	EXCLUDE_FROM_ALL
	DEPENDS dechamps_cpputil
)

check_git_submodule(ASIOTest)
ExternalProject_Add(
    ASIOTest
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/ASIOTest"
    BUILD_ALWAYS TRUE USES_TERMINAL_BUILD TRUE
    INSTALL_DIR "${INTERNAL_INSTALL_PREFIX}"
    CMAKE_ARGS ${CMAKE_ARGS}
	EXCLUDE_FROM_ALL
	DEPENDS cxxopts dechamps_cpputil dechamps_cpplog dechamps_ASIOUtil libsndfile
)

ExternalProject_Add(
    FlexASIO
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/flexasio"
    BUILD_ALWAYS TRUE USES_TERMINAL_BUILD TRUE
    INSTALL_DIR "${INTERNAL_INSTALL_PREFIX}"
    CMAKE_ARGS ${CMAKE_ARGS}
    DEPENDS tinytoml portaudio dechamps_cpputil dechamps_cpplog dechamps_ASIOUtil ASIOTest
)

install(DIRECTORY "${INTERNAL_INSTALL_PREFIX}/" DESTINATION "${CMAKE_INSTALL_PREFIX}")
install(SCRIPT dechamps_CMakeUtils/InstallPdbFiles.cmake)

# Work around https://developercommunity.visualstudio.com/content/problem/618088/cmake-msvc-toolset-version-is-incorrect-in-visual.html
# Note that for InstallRequiredSystemLibraries to work you might also need to work around https://developercommunity.visualstudio.com/content/problem/618084/cmake-installrequiredsystemlibraries-broken-in-lat.html
if (MSVC_VERSION EQUAL 1921 AND MSVC_TOOLSET_VERSION EQUAL 141)
	set(MSVC_TOOLSET_VERSION 142)
endif()
include(InstallRequiredSystemLibraries)
