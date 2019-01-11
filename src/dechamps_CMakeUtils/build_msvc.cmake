set(DECHAMPS_CMAKEUTILS_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(build_msvc)
	cmake_parse_arguments(BUILD_MSVC "" "SOURCE_DIR;BUILD_DIR;ARCH" "" ${ARGN})

	find_package(VisualStudio_VsDevCmd REQUIRED)
	find_package(VisualStudio_cmake REQUIRED)

	include(execute_process_or_die)

	# Used by build_msvc.bat
	set(ENV{DECHAMPS_CMAKEUTILS_VISUALSTUDIO_VSDEVCMD} "${VisualStudio_VsDevCmd_EXECUTABLE}")
	set(ENV{DECHAMPS_CMAKEUTILS_VISUALSTUDIO_CMAKE} "${VisualStudio_cmake_EXECUTABLE}")
	set(ENV{DECHAMPS_CMAKEUTILS_SOURCE_DIR} "${BUILD_MSVC_SOURCE_DIR}")

	execute_process_or_die(
		COMMAND cmd /D /C "${DECHAMPS_CMAKEUTILS_DIR}/build_msvc.bat" "${BUILD_MSVC_ARCH}"
		WORKING_DIRECTORY "${BUILD_MSVC_BUILD_DIR}"
	)

	unset(ENV{DECHAMPS_CMAKEUTILS_VISUALSTUDIO_VSDEVCMD})
	unset(ENV{DECHAMPS_CMAKEUTILS_VISUALSTUDIO_CMAKE})
	unset(ENV{DECHAMPS_CMAKEUTILS_SOURCE_DIR})
endfunction()
