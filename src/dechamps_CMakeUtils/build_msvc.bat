@rem Batch script for building FlexASIO in a specific configuration.
@rem Meant to be called as part of build_msvc.cmake, which will set the
@rem appropriate environment variables.
@rem
@rem The reason why this needs to be run by cmd.exe is because VsDevCmd.bat sets
@rem up environment variables that need to be inherited by the build process.
@rem installer.cmake could conceivably use a compound cmd.exe command line
@rem instead, but command line quoting/escaping issues makes that surprisingly
@rem hard to do in practice.

@set FLEXASIO_BUILD_ARCH=%1 || goto :error
@set FLEXASIO_INSTALL_DIR=%cd%\install || goto :error
@mkdir build || goto :error
@cd build || goto :error
call "%FLEXASIO_VISUALSTUDIO_VSDEVCMD%" -arch=%FLEXASIO_BUILD_ARCH% || goto :error
@echo on
"%FLEXASIO_VISUALSTUDIO_CMAKE%" -G Ninja -DCMAKE_BUILD_TYPE="Release" -DCMAKE_INSTALL_PREFIX:PATH="%FLEXASIO_INSTALL_DIR%" "%FLEXASIO_SOURCE_DIR%" || goto :error
"%FLEXASIO_VISUALSTUDIO_CMAKE%" --build . --target install || goto :error
@goto :EOF

:error
@echo %~dp0 failed with #%errorlevel%.
@exit /b %errorlevel%
