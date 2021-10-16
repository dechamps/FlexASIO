if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set(FLEXASIO_MIDL_ENV_FLAG /env win32)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(FLEXASIO_MIDL_ENV_FLAG /env amd64)
else()
	set(FLEXASIO_MIDL_ENV_FLAG)
endif()

add_custom_command(
	OUTPUT flexasio_h.h flexasio_i.c flexasio.tlb
	COMMAND midl /nologo /header flexasio_h.h ${FLEXASIO_MIDL_ENV_FLAG} "${CMAKE_CURRENT_LIST_DIR}/flexasio.idl"
	MAIN_DEPENDENCY flexasio.idl
)
add_library(FlexASIO_idl INTERFACE)
target_sources(FlexASIO_idl
	INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/flexasio_h.h"
	INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/flexasio.tlb"
)
target_include_directories(FlexASIO_idl INTERFACE "${CMAKE_CURRENT_BINARY_DIR}")

add_library(FlexASIO_cflexasio STATIC EXCLUDE_FROM_ALL cflexasio.cpp)
target_link_libraries(FlexASIO_cflexasio
	PRIVATE FlexASIO_flexasio
	PRIVATE FlexASIO_idl
	PRIVATE FlexASIO_log
	PRIVATE FlexASIOUtil_shell
	PRIVATE dechamps_cpputil::exception
	PRIVATE dechamps_ASIOUtil::asiosdk_iasiodrv
	PRIVATE dechamps_ASIOUtil::asio
)

add_library(FlexASIO_comdll STATIC EXCLUDE_FROM_ALL comdll.cpp)
target_compile_definitions(FlexASIO_comdll PRIVATE _WINDLL)

add_library(FlexASIO_config STATIC EXCLUDE_FROM_ALL config.cpp)
target_link_libraries(FlexASIO_config
	PRIVATE FlexASIO_log
	PRIVATE FlexASIOUtil_shell
	PRIVATE dechamps_cpputil::exception
	PRIVATE tinytoml
)

add_library(FlexASIO_control_panel STATIC EXCLUDE_FROM_ALL control_panel.cpp)
target_link_libraries(FlexASIO_control_panel
	PRIVATE FlexASIO_log
	PRIVATE FlexASIOUtil_windows_com
	PRIVATE FlexASIOUtil_windows_error
	PRIVATE FlexASIOUtil_windows_registry
	PRIVATE FlexASIOUtil_windows_string
	PRIVATE dechamps_CMakeUtils_version
	PRIVATE dechamps_cpputil::exception
)

add_library(FlexASIO_log STATIC EXCLUDE_FROM_ALL log.cpp)
target_link_libraries(FlexASIO_log
	PUBLIC dechamps_cpplog::log
	PRIVATE FlexASIOUtil_shell
	PRIVATE dechamps_CMakeUtils_version
)

add_library(FlexASIO_portaudio STATIC EXCLUDE_FROM_ALL portaudio.cpp)
target_link_libraries(FlexASIO_portaudio
	PRIVATE FlexASIOUtil_portaudio
	PRIVATE PortAudio::PortAudio
)

add_library(FlexASIO_flexasio STATIC EXCLUDE_FROM_ALL flexasio.cpp)
target_link_libraries(FlexASIO_flexasio
	PUBLIC dechamps_ASIOUtil::asiosdk_asioh
	PUBLIC dechamps_ASIOUtil::asiosdk_asiosys
	PUBLIC FlexASIO_config
	PUBLIC FlexASIOUtil_portaudio
	PRIVATE dechamps_ASIOUtil::asio
	PRIVATE FlexASIO_control_panel
	PRIVATE FlexASIO_log
	PRIVATE dechamps_cpputil::endian
	PRIVATE dechamps_cpputil::exception
	PRIVATE dechamps_cpputil::string
	PRIVATE PortAudio::PortAudio
	PRIVATE winmm
)

# Note: this is SHARED, not MODULE, otherwise CMake refuses to link that in FlexASIOTest.
add_library(FlexASIO SHARED dll.def flexasio.rc ../versioninfo.rc flexasio.manifest)
target_compile_definitions(FlexASIO PRIVATE PROJECT_DESCRIPTION="FlexASIO ASIO Driver DLL")
target_link_libraries(FlexASIO
	PRIVATE FlexASIO_cflexasio
	PRIVATE FlexASIO_portaudio
	PRIVATE dechamps_CMakeUtils_version_stamp
	PRIVATE FlexASIO_comdll
	PRIVATE FlexASIO_idl
)
install(TARGETS FlexASIO RUNTIME DESTINATION bin)
