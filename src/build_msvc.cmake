function(build_msvc)
	cmake_parse_arguments(BUILD_MSVC "" "SOURCE_DIR;BUILD_DIR;ARCH" "" ${ARGN})

	list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/dechamps_CMakeUtils")
	find_package(VisualStudio_VsDevCmd REQUIRED)
	find_package(VisualStudio_cmake REQUIRED)

	include(dechamps_CMakeUtils/execute_process_or_die.cmake)

	# Used by build_msvc.bat
	set(ENV{FLEXASIO_VISUALSTUDIO_VSDEVCMD} "${VisualStudio_VsDevCmd_EXECUTABLE}")
	set(ENV{FLEXASIO_VISUALSTUDIO_CMAKE} "${VisualStudio_cmake_EXECUTABLE}")
	set(ENV{FLEXASIO_SOURCE_DIR} "${BUILD_MSVC_SOURCE_DIR}")

	execute_process_or_die(
		COMMAND cmd /D /C "${CMAKE_CURRENT_LIST_DIR}/build_msvc.bat" "${BUILD_MSVC_ARCH}"
		WORKING_DIRECTORY "${BUILD_MSVC_BUILD_DIR}"
	)

	unset(ENV{FLEXASIO_VISUALSTUDIO_VSDEVCMD})
	unset(ENV{FLEXASIO_VISUALSTUDIO_CMAKE})
	unset(ENV{FLEXASIO_SOURCE_DIR})
endfunction()
