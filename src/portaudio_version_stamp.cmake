if (Git_FOUND)
	execute_process(
		COMMAND "${GIT_EXECUTABLE}" -C "${PORTAUDIO_LIST_DIR}" rev-parse HEAD
		OUTPUT_VARIABLE PA_GIT_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE
	)
else()
	set(PA_GIT_REVISION "unknown")
endif()
configure_file("${CMAKE_CURRENT_LIST_DIR}/portaudio_version_stamp.in.h" "${OUTPUT_HEADER_FILE}" @ONLY)
