# FlexASIO Developer Information

FlexASIO is written for the Microsoft Visual C++ 2017 toolchain.

For the build to work you will need to provide these dependencies:

 - [PortAudio][]
 - [tinytoml][]

The best way to get these dependencies is to use [vcpkg][]:

```
vcpkg install portaudio:x64-windows portaudio:x86-windows tinytoml:x64-windows tinytoml:x86-windows
```

You will also need to provide the [ASIO SDK][]. Download the SDK and put the
ASIOSDK2.3.1 folder inside the FlexASIO `src` folder.

The installer can be built using [Inno Setup][].

[ASIO SDK]: http://www.steinberg.net/en/company/developer.html
[Inno Setup]: http://www.jrsoftware.org/isdl.php
[PortAudio]: http://www.portaudio.com/
[tinytoml]: https://github.com/mayah/tinytoml
[vcpkg]: https://github.com/Microsoft/vcpkg
