[Setup]
AppID=FlexASIO
AppName=FlexASIO
AppVerName=FlexASIO 0.1
AppVersion=0.1
AppPublisher=Etienne Dechamps
AppPublisherURL=https://github.com/dechamps/FlexASIO
AppSupportURL=https://github.com/dechamps/FlexASIO
AppUpdatesURL=https://github.com/dechamps/FlexASIO
AppContact=etienne@edechamps.fr

OutputDir=.
OutputBaseFilename=FlexASIO-0.1

DefaultDirName={pf}\FlexASIO
AppendDefaultDirName=no

LicenseFile=LICENSE.txt

[Files]
Source:"Release\FlexASIO.dll"; DestDir: "{app}"; Flags: ignoreversion regserver 32bit
Source:"LICENSE.txt"; DestDir:"{app}"; Flags: ignoreversion

; PortAudio library, 32-bit DLL.
Source:"redist\portaudio_x86.dll"; DestDir: "{app}"; Flags: ignoreversion
; Microsoft Visual C++ 2010 SP1 Redistributable Package (x86)
; From: (Visual Studio 2010 install dir)\VC\redist\x86\Microsoft.VC100.CRT
Source:"redist\msvcp100.dll"; DestDir: "{app}"; Flags: ignoreversion
Source:"redist\msvcr100.dll"; DestDir: "{app}"; Flags: ignoreversion
 