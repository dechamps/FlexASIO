add_library(FlexASIOUtil_portaudio STATIC portaudio.cpp)
target_link_libraries(FlexASIOUtil_portaudio
	PUBLIC portaudio::portaudio
	PRIVATE dechamps_cpputil::string
)

add_library(FlexASIOUtil_shell STATIC shell.cpp)
