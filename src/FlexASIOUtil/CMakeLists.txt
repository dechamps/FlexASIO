add_library(FlexASIOUtil_asio STATIC asio.cpp)
target_link_libraries(FlexASIOUtil_asio
	PRIVATE FlexASIOUtil_endian
)

add_library(FlexASIOUtil_endian STATIC endian.cpp) # TODO: use CMake TestBigEndian module

add_library(FlexASIOUtil_log STATIC log.cpp)
target_link_libraries(FlexASIOUtil_log
	PRIVATE FlexASIOUtil_shell
	PRIVATE FlexASIOUtil_version
)

add_library(FlexASIOUtil_portaudio STATIC portaudio.cpp)
target_link_libraries(FlexASIOUtil_portaudio PUBLIC portaudio::portaudio)

add_library(FlexASIOUtil_shell STATIC shell.cpp)

add_library(FlexASIOUtil_version STATIC version.cpp)
target_link_libraries(FlexASIOUtil_version PRIVATE FlexASIO_build_version)
