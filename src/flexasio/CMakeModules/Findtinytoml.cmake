find_path(tinytoml_INCLUDE_DIR NAMES toml/toml.h)
mark_as_advanced(tinytoml_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(tinytoml REQUIRED_VARS tinytoml_INCLUDE_DIR)

if(tinytoml_FOUND AND NOT TARGET tinytoml)
	add_library(tinytoml INTERFACE)
	target_include_directories(tinytoml INTERFACE "${tinytoml_INCLUDE_DIR}")
endif()
