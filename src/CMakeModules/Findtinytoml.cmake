find_path(tinytoml_INCLUDE_DIR NAMES toml/toml.h)
mark_as_advanced(tinytoml_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(tinytoml REQUIRED_VARS tinytoml_INCLUDE_DIR)

if(tinytoml_FOUND AND NOT TARGET tinytoml::toml)
	add_library(tinytoml::toml INTERFACE IMPORTED)
	set_target_properties(tinytoml::toml PROPERTIES
		INTERFACE_INCLUDE_DIRECTORIES "${tinytoml_INCLUDE_DIR}"
	)
endif()
