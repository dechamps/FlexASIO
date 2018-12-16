# See https://github.com/Microsoft/vswhere

set(_PROGRAM_FILES "ProgramFiles")
set(_PROGRAM_FILES_x86 "ProgramFiles(x86)")
find_program(vswhere_EXECUTABLE vswhere
    HINTS "$ENV{${_PROGRAM_FILES_x86}}/Microsoft Visual Studio/Installer" "$ENV{${_PROGRAM_FILES}}/Microsoft Visual Studio/Installer"
)
mark_as_advanced(vswhere_EXECUTABLE)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(vswhere REQUIRED_VARS vswhere_EXECUTABLE)
