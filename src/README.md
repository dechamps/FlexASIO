# FlexASIO Developer Information

FlexASIO is designed to use the Microsoft Visual C++ 2017 toolchain.

For the build to work you will need to provide these dependencies:

 - [PortAudio](http://www.portaudio.com/)
 - [tinytoml](https://github.com/mayah/tinytoml)

The best way to get these dependencies is to use [vcpkg](https://github.com/Microsoft/vcpkg):

```
vcpkg install portaudio:x64-windows portaudio:x86-windows tinytoml:x64-windows tinytoml:x86-windows
```

You will also need to provide the ASIO SDK.
[Download](http://www.steinberg.net/en/company/developer.html) the SDK
and copy the ASIOSDK2.3.1 folder to the `src` folder.

The installer can be built using
[Inno Setup](http://www.jrsoftware.org/isdl.php).
