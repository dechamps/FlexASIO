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

; Microsoft Visual C++ 2017 runtime
; From: (Visual Studio 2017 install dir)\Community\VC\Redist\MSVC\14.15.26706
Source: "redist\vcredist_x86.exe"; DestDir: {tmp}; Flags: deleteafterinstall

[Run]
Filename:"{tmp}\vcredist_x86.exe"; Parameters: "/passive"; StatusMsg: "Installing Microsoft Visual C++ 2017 runtime"
 