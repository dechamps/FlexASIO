find_package(vswhere MODULE REQUIRED)

execute_process(
    COMMAND "${vswhere_EXECUTABLE}" -requires Microsoft.VisualStudio.Workload.NativeDesktop -format value -property installationPath
    OUTPUT_VARIABLE VSDIR OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(VisualStudio_ROOT_DIR "${VSDIR}" CACHE PATH "Visual Studio installation directory")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VisualStudio REQUIRED_VARS VisualStudio_ROOT_DIR)
