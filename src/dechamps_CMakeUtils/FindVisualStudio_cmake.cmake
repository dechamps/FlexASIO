find_package(VisualStudio MODULE REQUIRED)

find_program(VisualStudio_cmake_EXECUTABLE cmake
    HINTS "${VisualStudio_ROOT_DIR}/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin"
    NO_DEFAULT_PATH
)
mark_as_advanced(VisualStudio_cmake_EXECUTABLE)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VisualStudio_cmake REQUIRED_VARS VisualStudio_cmake_EXECUTABLE)
