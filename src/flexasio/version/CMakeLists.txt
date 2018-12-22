set(FLEXASIO_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/version_stamp.h")
add_custom_target(FlexASIO_version_stamp_gen
	COMMAND "${CMAKE_COMMAND}" -DOUTPUT_HEADER_FILE="${FLEXASIO_VERSION_FILE}" -DGit_FOUND="${Git_FOUND}" -DGIT_EXECUTABLE="${GIT_EXECUTABLE}" -P "${CMAKE_CURRENT_LIST_DIR}/version_stamp.cmake"
	BYPRODUCTS "${FLEXASIO_VERSION_FILE}"
)
add_library(FlexASIO_version_stamp INTERFACE)
add_dependencies(FlexASIO_version_stamp FlexASIO_version_stamp_gen)
target_include_directories(FlexASIO_version_stamp
	INTERFACE "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(FlexASIO_version STATIC version.cpp)
target_link_libraries(FlexASIO_version PRIVATE FlexASIO_version_stamp)
