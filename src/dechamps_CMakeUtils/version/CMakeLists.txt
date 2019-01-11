set(DECHAMPS_CMAKEUTILS_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")

configure_file(version.h "${DECHAMPS_CMAKEUTILS_INCLUDE_DIR}/dechamps_CMakeUtils/version.h" COPYONLY)

find_package(Git)
set(DECHAMPS_CMAKEUTILS_VERSION_STAMP_FILE "${DECHAMPS_CMAKEUTILS_INCLUDE_DIR}/dechamps_CMakeUtils/version_stamp.h")
add_custom_target(dechamps_CMakeUtils_version_stamp_gen
	COMMAND "${CMAKE_COMMAND}" -DOUTPUT_HEADER_FILE="${DECHAMPS_CMAKEUTILS_VERSION_STAMP_FILE}" -DGit_FOUND="${Git_FOUND}" -DGIT_EXECUTABLE="${GIT_EXECUTABLE}" -P "${CMAKE_CURRENT_LIST_DIR}/version_stamp.cmake"
	BYPRODUCTS "${DECHAMPS_CMAKEUTILS_VERSION_STAMP_FILE}"
)
add_library(dechamps_CMakeUtils_version_stamp INTERFACE)
add_dependencies(dechamps_CMakeUtils_version_stamp dechamps_CMakeUtils_version_stamp_gen)
target_include_directories(dechamps_CMakeUtils_version_stamp
	INTERFACE "${DECHAMPS_CMAKEUTILS_INCLUDE_DIR}"
)

add_library(dechamps_CMakeUtils_version STATIC version.c)
target_link_libraries(dechamps_CMakeUtils_version PRIVATE dechamps_CMakeUtils_version_stamp)
target_include_directories(dechamps_CMakeUtils_version
	PUBLIC "${DECHAMPS_CMAKEUTILS_INCLUDE_DIR}"
)
