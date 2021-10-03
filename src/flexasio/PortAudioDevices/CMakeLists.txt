add_executable(PortAudioDevices list.cpp ../versioninfo.rc)
target_compile_definitions(PortAudioDevices PRIVATE PROJECT_DESCRIPTION="PortAudio device list application")
target_link_libraries(PortAudioDevices
	PRIVATE dechamps_CMakeUtils_version_stamp
	PRIVATE FlexASIOUtil_portaudio
	PRIVATE dechamps_cpputil::string
	PRIVATE PortAudio::PortAudio
)
install(TARGETS PortAudioDevices RUNTIME DESTINATION bin)
