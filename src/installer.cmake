list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/dechamps_CMakeUtils")
find_package(VisualStudio_VsDevCmd REQUIRED)
find_package(VisualStudio_cmake REQUIRED)
find_package(InnoSetup REQUIRED)

find_package(Git REQUIRED)
include(dechamps_CMakeUtils/version/version.cmake)
string(REGEX REPLACE "^flexasio-" "" FLEXASIO_VERSION "${DECHAMPS_CMAKEUTILS_GIT_DESCRIPTION_DIRTY}")

string(TIMESTAMP FLEXASIO_BUILD_TIMESTAMP "%Y-%m-%dT%H%M%SZ" UTC)
string(RANDOM FLEXASIO_BUILD_ID)
if (NOT DEFINED FLEXASIO_BUILD_ROOT_DIR)
    set(FLEXASIO_BUILD_ROOT_DIR "$ENV{USERPROFILE}/CMakeBuilds/FlexASIO-${FLEXASIO_BUILD_TIMESTAMP}-${FLEXASIO_BUILD_ID}")
endif()
message(STATUS "FlexASIO build root directory: ${FLEXASIO_BUILD_ROOT_DIR}")

file(MAKE_DIRECTORY "${FLEXASIO_BUILD_ROOT_DIR}/x64" "${FLEXASIO_BUILD_ROOT_DIR}/x86")

include(dechamps_CMakeUtils/execute_process_or_die.cmake)

# Used by installer_build.bat
set(ENV{FLEXASIO_VISUALSTUDIO_VSDEVCMD} "${VisualStudio_VsDevCmd_EXECUTABLE}")
set(ENV{FLEXASIO_VISUALSTUDIO_CMAKE} "${VisualStudio_cmake_EXECUTABLE}")
set(ENV{FLEXASIO_SOURCE_DIR} "${CMAKE_CURRENT_LIST_DIR}")

execute_process_or_die(
    COMMAND cmd /D /C "${CMAKE_CURRENT_LIST_DIR}/installer_build.bat" amd64
    WORKING_DIRECTORY "${FLEXASIO_BUILD_ROOT_DIR}/x64"
)
execute_process_or_die(
    COMMAND cmd /D /C "${CMAKE_CURRENT_LIST_DIR}/installer_build.bat" x86
    WORKING_DIRECTORY "${FLEXASIO_BUILD_ROOT_DIR}/x86"
)

file(GLOB FLEXASIO_DOC_FILES LIST_DIRECTORIES FALSE "${CMAKE_CURRENT_LIST_DIR}/../*.txt" "${CMAKE_CURRENT_LIST_DIR}/../*.md")
file(INSTALL ${FLEXASIO_DOC_FILES} DESTINATION "${FLEXASIO_BUILD_ROOT_DIR}")

configure_file("${CMAKE_CURRENT_LIST_DIR}/installer.in.iss" "${FLEXASIO_BUILD_ROOT_DIR}/installer.iss" @ONLY)
execute_process_or_die(
    COMMAND "${InnoSetup_iscc_EXECUTABLE}" "${FLEXASIO_BUILD_ROOT_DIR}/installer.iss" /O. /FFlexASIO-${FLEXASIO_VERSION}
    WORKING_DIRECTORY "${FLEXASIO_BUILD_ROOT_DIR}"
)
