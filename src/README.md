# FlexASIO Developer Information

## Building

FlexASIO is designed to be built using CMake within the Microsoft Visual C++
2017 toolchain native CMake support.

FlexASIO uses a CMake "superbuild" system (in `/src`) to automatically build the
[tinytoml][] and [PortAudio][] dependencies before building FlexASIO itself.
These dependencies are pulled in as git submodules; make sure to run
`git submodule update --init`.

If you want to handle the dependencies yourself, you can build FlexASIO in
isolation from `/src/flexasio`.

In any case, the FlexASIO CMake build system will download the [ASIO SDK][] for
you automatically at configure time.

## Packaging

The following command will do a clean superbuild and generate a FlexASIO
installer package for you:

```
cmake -P installer.cmake
```

Note that for this command to work, you need to have [Inno Setup][] installed.

[ASIO SDK]: http://www.steinberg.net/en/company/developer.html
[Inno Setup]: http://www.jrsoftware.org/isdl.php
[PortAudio]: http://www.portaudio.com/
[tinytoml]: https://github.com/mayah/tinytoml
