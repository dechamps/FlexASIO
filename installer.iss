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
ArchitecturesInstallIn64BitMode=x64

LicenseFile=LICENSE.txt

[Files]
Source:"x64\Release\FlexASIO_x64.dll"; DestDir: "{app}"; Flags: ignoreversion regserver 64bit; Check: Is64BitInstallMode
Source:"Release\FlexASIO_x86.dll"; DestDir: "{app}"; Flags: ignoreversion regserver 32bit
Source:"LICENSE.txt"; DestDir:"{app}"; Flags: ignoreversion

; PortAudio library
Source:"redist\portaudio_x64.dll"; DestDir: "{app}"; Flags: ignoreversion 64bit; Check: Is64BitInstallMode
Source:"redist\portaudio_x86.dll"; DestDir: "{app}"; Flags: ignoreversion 32bit

; Microsoft Visual C++ 2017 runtime
; From: (Visual Studio 2017 install dir)\Community\VC\Redist\MSVC\14.15.26706
Source: "redist\vcredist_x64.exe"; DestDir: {tmp}; Flags: deleteafterinstall 64bit; Check: Is64BitInstallMode
Source: "redist\vcredist_x86.exe"; DestDir: {tmp}; Flags: deleteafterinstall 32bit

[Run]
Filename:"{tmp}\vcredist_x64.exe"; Parameters: "/passive"; StatusMsg: "Installing Microsoft Visual C++ 2017 runtime (x64)"; Flags: 64bit; Check: Is64BitInstallMode
Filename:"{tmp}\vcredist_x86.exe"; Parameters: "/passive"; StatusMsg: "Installing Microsoft Visual C++ 2017 runtime (x86)"; Flags: 32bit
 