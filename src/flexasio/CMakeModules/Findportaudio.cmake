find_path(portaudio_INCLUDE_DIR	NAMES portaudio.h)
find_library(portaudio_LIBRARY NAMES "portaudio_${FLEXASIO_PLATFORM}")
mark_as_advanced(portaudio_INCLUDE_DIR portaudio_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(portaudio REQUIRED_VARS portaudio_INCLUDE_DIR portaudio_LIBRARY)

if(portaudio_FOUND AND NOT TARGET portaudio::portaudio)
	add_library(portaudio::portaudio UNKNOWN IMPORTED)
		set_target_properties(portaudio::portaudio PROPERTIES
		IMPORTED_LOCATION "${portaudio_LIBRARY}"
		INTERFACE_INCLUDE_DIRECTORIES "${portaudio_INCLUDE_DIR}"
	)
endif()
