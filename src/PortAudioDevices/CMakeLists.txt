add_executable(PortAudioDevices list.cpp)
target_link_libraries(PortAudioDevices
	PRIVATE FlexASIOUtil_log
	PRIVATE FlexASIOUtil_portaudio
	PRIVATE portaudio::portaudio
)
