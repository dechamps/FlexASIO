[Setup]
AppID=ASIOBridge
AppName=ASIOBridge
AppVerName=ASIOBridge 0.1
AppVersion=0.1
AppPublisher=Etienne Dechamps
AppPublisherURL=https://github.com/dechamps/ASIOBridge
AppSupportURL=https://github.com/dechamps/ASIOBridge
AppUpdatesURL=https://github.com/dechamps/ASIOBridge
AppContact=etienne@edechamps.fr

OutputDir=.
OutputBaseFilename=ASIOBridge-0.1

DefaultDirName={pf}\ASIOBridge
AppendDefaultDirName=no

LicenseFile=LICENSE.txt

[Files]
Source:"Release\ASIOBridge.dll"; DestDir: "{app}"; Flags: ignoreversion regserver 32bit
Source:"LICENSE.txt"; DestDir:"{app}"; Flags: ignoreversion

; PortAudio library, 32-bit DLL.
Source:"redist\portaudio_x86.dll"; DestDir: "{app}"; Flags: ignoreversion
; Microsoft Visual C++ 2010 SP1 Redistributable Package (x86)
; From: (Visual Studio 2010 install dir)\VC\redist\x86\Microsoft.VC100.CRT
Source:"redist\msvcp100.dll"; DestDir: "{app}"; Flags: ignoreversion
Source:"redist\msvcr100.dll"; DestDir: "{app}"; Flags: ignoreversion
 