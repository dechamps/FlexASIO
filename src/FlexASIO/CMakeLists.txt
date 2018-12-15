if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set(FLEXASIO_MIDL_ENV_FLAG /env win32)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(FLEXASIO_MIDL_ENV_FLAG /env amd64)
else()
	set(FLEXASIO_MIDL_ENV_FLAG)
endif()

# TODO: missing TLB output
add_custom_command(
	OUTPUT flexasio_h.h
	COMMAND midl /nologo /header flexasio_h.h ${FLEXASIO_MIDL_ENV_FLAG} "${CMAKE_CURRENT_LIST_DIR}/flexasio.idl"
	MAIN_DEPENDENCY flexasio.idl
)

add_library(FlexASIO_cflexasio cflexasio.cpp flexasio.idl)
target_include_directories(FlexASIO_cflexasio PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(FlexASIO_cflexasio
	PRIVATE FlexASIO_flexasio
	PRIVATE FlexASIOUtil_asio
	PRIVATE FlexASIOUtil_log
	PRIVATE FlexASIOUtil_shell
)

add_library(FlexASIO_comdll comdll.cpp)
target_compile_definitions(FlexASIO_comdll PRIVATE _WINDLL)

add_library(FlexASIO_config config.cpp)
target_link_libraries(FlexASIO_config
	PRIVATE FlexASIOUtil_log
	PRIVATE FlexASIOUtil_shell
	PRIVATE portaudio::portaudio # TODO: use a cleaner way to find tinytoml
)

add_library(FlexASIO_flexasio flexasio.cpp)
target_link_libraries(FlexASIO_flexasio
	PUBLIC FlexASIO_config
	PUBLIC FlexASIOUtil_portaudio
	PRIVATE FlexASIOUtil_asio
	PRIVATE FlexASIOUtil_log
	PRIVATE portaudio::portaudio
	PRIVATE winmm
)

# Note: this is SHARED, not MODULE, otherwise CMake refuses to link that in FlexASIOTest.
add_library(FlexASIO SHARED dll.def flexasio.rc flexasio.idl)
target_include_directories(FlexASIO PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(FlexASIO
	PRIVATE FlexASIO_cflexasio
	PRIVATE FlexASIO_comdll
)
