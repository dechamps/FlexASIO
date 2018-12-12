add_custom_command(
	OUTPUT flexasio_h.h
	COMMAND midl /nologo /header flexasio_h.h /env x64 "${CMAKE_CURRENT_LIST_DIR}/flexasio.idl"  # TODO: fix hardcoded architecture
	MAIN_DEPENDENCY flexasio.idl
)

# Note: this is SHARED, not MODULE, otherwise CMake refuses to link that in FlexASIOTest.
add_library(FlexASIO SHARED cflexasio.cpp comdll.cpp config.cpp dll.def flexasio.cpp flexasio.idl flexasio.rc)
target_compile_definitions(FlexASIO PRIVATE _WINDLL)
target_include_directories(FlexASIO PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(FlexASIO
	PRIVATE portaudio::portaudio
	PRIVATE FlexASIOUtil_asio
	PRIVATE FlexASIOUtil_endian
	PRIVATE FlexASIOUtil_log
	PRIVATE FlexASIOUtil_portaudio
	PRIVATE winmm
)
