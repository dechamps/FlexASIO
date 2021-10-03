add_library(FlexASIOUtil_portaudio STATIC portaudio.cpp)
target_link_libraries(FlexASIOUtil_portaudio
	PUBLIC PortAudio::PortAudio
	PRIVATE dechamps_cpputil::string
)

add_library(FlexASIOUtil_shell STATIC shell.cpp)
