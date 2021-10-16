add_library(FlexASIOUtil_portaudio STATIC portaudio.cpp)
target_link_libraries(FlexASIOUtil_portaudio
	PUBLIC PortAudio::PortAudio
	PRIVATE dechamps_cpputil::string
)

add_library(FlexASIOUtil_shell STATIC shell.cpp)

add_library(FlexASIOUtil_windows_com STATIC windows_com.cpp)
target_link_libraries(FlexASIOUtil_windows_com
	PRIVATE FlexASIOUtil_windows_error
)

add_library(FlexASIOUtil_windows_error STATIC windows_error.cpp)

add_library(FlexASIOUtil_windows_registry STATIC windows_registry.cpp)
target_link_libraries(FlexASIOUtil_windows_registry
	PRIVATE FlexASIOUtil_windows_error
)

add_library(FlexASIOUtil_windows_string STATIC windows_string.cpp)
target_link_libraries(FlexASIOUtil_windows_string
	PRIVATE FlexASIOUtil_windows_error
)
