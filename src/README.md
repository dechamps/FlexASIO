# FlexASIO Developer Information

## Building

FlexASIO is designed to be built using CMake within the Microsoft Visual C++
2017 toolchain native CMake support.

For the build to work you will need to provide the [PortAudio][] dependency. The
easiest way is to use [vcpkg][]:

```
vcpkg install portaudio:x64-windows portaudio:x86-windows
```

Note that vcpkg needs to be integrated with CMake for the build to work. You
can use the method described in the vcpkg documentation, or you might find it
easier to simply set the `VCPKG_DIR` environment variable to your vcpkg
directory.

The FlexASIO CMake build system will download the [ASIO SDK][] for you
automatically at configure time.

The [tinytoml][] dependency is pulled in as a git submodule. Make sure to
run `git submodule update --init`.

## Packaging

The following command will do a clean build and generate a FlexASIO installer
package for you:

```
cmake -P installer.cmake
```

Note that for this command to work, you need to have [Inno Setup][] installed,
and the `VCPKG_DIR` environment variable must be set.

[ASIO SDK]: http://www.steinberg.net/en/company/developer.html
[Inno Setup]: http://www.jrsoftware.org/isdl.php
[PortAudio]: http://www.portaudio.com/
[tinytoml]: https://github.com/mayah/tinytoml
[vcpkg]: https://github.com/Microsoft/vcpkg
