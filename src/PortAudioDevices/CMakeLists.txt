add_executable(PortAudioDevices list.cpp ../versioninfo.rc)
target_compile_definitions(PortAudioDevices PRIVATE PROJECT_DESCRIPTION="PortAudio device list application")
target_link_libraries(PortAudioDevices
	PRIVATE FlexASIO_build_version
	PRIVATE FlexASIOUtil_log
	PRIVATE FlexASIOUtil_portaudio
	PRIVATE portaudio::portaudio
)
install(TARGETS PortAudioDevices RUNTIME DESTINATION bin)
