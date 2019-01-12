find_package(VisualStudio MODULE REQUIRED)

find_program(VisualStudio_VsDevCmd_EXECUTABLE VsDevCmd.bat
    HINTS "${VisualStudio_ROOT_DIR}/Common7/Tools"
    NO_DEFAULT_PATH
)
mark_as_advanced(VisualStudio_VsDevCmd_EXECUTABLE)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VisualStudio_VsDevCmd REQUIRED_VARS VisualStudio_VsDevCmd_EXECUTABLE)
