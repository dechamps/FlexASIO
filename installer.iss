#define FIND_VS_RESULT Exec("powershell", "-ExecutionPolicy Bypass -NoProfile -NonInteractive -File find_visual_studio.ps1", SourcePath, , SW_HIDE)
#if FIND_VS_RESULT != 0
#error Unable to find Visual Studio directory
#endif
#include "build\visual_studio.h"

#define BUILD_X64_RESULT Exec(VISUAL_STUDIO_INSTALLATION_PATH + "\MSBuild\15.0\Bin\MSBuild.exe", "-property:Configuration=Release;Platform=x64 /t:Clean,Build", SourcePath)
#if BUILD_X64_RESULT != 0
#error Unable to build solution for X64
#endif

#define BUILD_X86_RESULT Exec(VISUAL_STUDIO_INSTALLATION_PATH + "\MSBuild\15.0\Bin\MSBuild.exe", "-property:Configuration=Release;Platform=Win32 /t:Clean,Build", SourcePath)
#if BUILD_X86_RESULT != 0
#error Unable to build solution for X86
#endif

#include "build\version.h"

[Setup]
AppID=FlexASIO
AppName=FlexASIO
AppVerName=FlexASIO {#FLEXASIO_VERSION}
AppVersion={#FLEXASIO_VERSION}
AppPublisher=Etienne Dechamps
AppPublisherURL=https://github.com/dechamps/FlexASIO
AppSupportURL=https://github.com/dechamps/FlexASIO/issues
AppUpdatesURL=https://github.com/dechamps/FlexASIO/releases
AppReadmeFile=https://github.com/dechamps/FlexASIO/blob/{#FLEXASIO_GITSTR}/README.md
AppContact=etienne@edechamps.fr

OutputDir=build
OutputBaseFilename=FlexASIO-{#FLEXASIO_VERSION}

DefaultDirName={pf}\FlexASIO
AppendDefaultDirName=no
ArchitecturesInstallIn64BitMode=x64

LicenseFile=LICENSE.txt

[Files]
Source:"build\x64\Release\FlexASIO_x64.dll"; DestDir: "{app}\x64"; Flags: ignoreversion regserver 64bit; Check: Is64BitInstallMode
Source:"build\x64\Release\FlexASIO_x64.pdb"; DestDir: "{app}\x64"; Flags: ignoreversion 64bit; Check: Is64BitInstallMode
Source:"build\x64\Release\FlexASIOTest_x64.exe"; DestDir: "{app}\x64"; Flags: ignoreversion 64bit; Check: Is64BitInstallMode
Source:"build\x86\Release\FlexASIO_x86.dll"; DestDir: "{app}\x86"; Flags: ignoreversion regserver 32bit
Source:"build\x86\Release\FlexASIO_x86.pdb"; DestDir: "{app}\x86"; Flags: ignoreversion 32bit
Source:"build\x86\Release\FlexASIOTest_x86.exe"; DestDir: "{app}\x86"; Flags: ignoreversion 32bit
Source:"LICENSE.txt"; DestDir:"{app}"; Flags: ignoreversion
Source:"README.md"; DestDir:"{app}"; Flags: ignoreversion

; PortAudio library.
; Note that these are not actually build artefacts; you need to put them there manually.
; The reason why this is fetching these DLLs from build is because it's convenient to have them there during testing.
Source:"build\x64\Release\portaudio_x64.dll"; DestDir: "{app}\x64"; Flags: ignoreversion 64bit; Check: Is64BitInstallMode
Source:"build\x64\Release\portaudio_x64.pdb"; DestDir: "{app}\x64"; Flags: ignoreversion 64bit; Check: Is64BitInstallMode
Source:"build\x86\Release\portaudio_x86.dll"; DestDir: "{app}\x86"; Flags: ignoreversion 32bit
Source:"build\x86\Release\portaudio_x86.pdb"; DestDir: "{app}\x86"; Flags: ignoreversion 32bit

Source: "{#VISUAL_STUDIO_REDIST_PATH}\x64\Microsoft.VC141.CRT\msvcp140.dll"; DestDir: "{app}\x64"; Flags: 64bit; Check: Is64BitInstallMode
Source: "{#VISUAL_STUDIO_REDIST_PATH}\x64\Microsoft.VC141.CRT\vcruntime140.dll"; DestDir: "{app}\x64"; Flags: 64bit; Check: Is64BitInstallMode
Source: "{#VISUAL_STUDIO_REDIST_PATH}\x86\Microsoft.VC141.CRT\msvcp140.dll"; DestDir: "{app}\x86"; Flags: 32bit
Source: "{#VISUAL_STUDIO_REDIST_PATH}\x86\Microsoft.VC141.CRT\vcruntime140.dll"; DestDir: "{app}\x86"; Flags: 32bit

[Run]
Filename:"https://github.com/dechamps/FlexASIO/blob/{#FLEXASIO_GITSTR}/README.md"; Description:"Open README"; Flags: postinstall shellexec nowait
 