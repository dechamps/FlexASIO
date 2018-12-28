add_library(FlexASIOUtil_asio STATIC asio.cpp)
target_link_libraries(FlexASIOUtil_asio
	PUBLIC asiosdk_headers
	PRIVATE FlexASIO_endian
)

add_library(FlexASIOUtil_log STATIC log.cpp)

add_library(FlexASIOUtil_portaudio STATIC portaudio.cpp)
target_link_libraries(FlexASIOUtil_portaudio PUBLIC portaudio::portaudio)

add_library(FlexASIOUtil_shell STATIC shell.cpp)
