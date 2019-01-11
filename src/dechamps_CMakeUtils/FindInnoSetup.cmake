find_program(InnoSetup_iscc_EXECUTABLE iscc
    HINTS
        # Sadly, Inno Setup doesn't seem to have a clean registry entry for the
        # installation path. This is the best we got.
        "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Inno Setup 5_is1;InstallLocation]"
        "[HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Inno Setup 5_is1;InstallLocation]"
)
mark_as_advanced(InnoSetup_iscc_EXECUTABLE)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(InnoSetup REQUIRED_VARS InnoSetup_iscc_EXECUTABLE)
