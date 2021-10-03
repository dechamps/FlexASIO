include(check_git_submodule.cmake)
check_git_submodule(dechamps_CMakeUtils)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/dechamps_CMakeUtils")
find_package(InnoSetup MODULE REQUIRED)

find_package(Git MODULE REQUIRED)
set(DECHAMPS_CMAKEUTILS_GIT_DIR "${CMAKE_CURRENT_LIST_DIR}/flexasio")
include(version/version)
string(REGEX REPLACE "^flexasio-" "" FLEXASIO_VERSION "${DECHAMPS_CMAKEUTILS_GIT_DESCRIPTION_DIRTY}")

configure_file("${CMAKE_CURRENT_LIST_DIR}/installer.in.iss" "${CMAKE_CURRENT_LIST_DIR}/out/installer.iss" @ONLY)
include(execute_process_or_die)
execute_process_or_die(
    COMMAND "${InnoSetup_iscc_EXECUTABLE}" out/installer.iss /Oout/installer /FFlexASIO-${FLEXASIO_VERSION}
)
