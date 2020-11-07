# FlexASIO Developer Information

See `LICENSE.txt` for licensing information. In particular, do note that
specific license terms apply to the ASIO trademark and ASIO SDK.

## Building

FlexASIO is designed to be built using CMake within the Microsoft Visual C++
2019 toolchain native CMake support.

FlexASIO uses a CMake "superbuild" system (in `/src`) to automatically build the
dependencies (most notably [PortAudio][]) before building FlexASIO itself. These
dependencies are pulled in as git submodules.

It is strongly recommended to use the superbuild system. Providing dependencies
manually is quite tedious because FlexASIO uses a highly modular structure that
relies on many small subprojects.

Note that the ASIOUtil build system will download the [ASIO SDK][] for you
automatically at configure time.

## Packaging

The following command will do a clean superbuild and generate a FlexASIO
installer package for you:

```
cmake -P installer.cmake
```

Note that for this command to work, you need to have [Inno Setup][] installed.

## Troubleshooting

### VC runtime DLLs are not included in the installation

See this [Visual Studio 2019 bug][InstallRequiredSystemLibraries].

---

*ASIO is a trademark and software of Steinberg Media Technologies GmbH*

[ASIO SDK]: http://www.steinberg.net/en/company/developer.html
[Inno Setup]: http://www.jrsoftware.org/isdl.php
[InstallRequiredSystemLibraries]: https://developercommunity.visualstudio.com/content/problem/618084/cmake-installrequiredsystemlibraries-broken-in-lat.html
[PortAudio]: http://www.portaudio.com/
[tinytoml]: https://github.com/mayah/tinytoml
