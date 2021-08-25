; Note: this Inno Setup installer script is meant to run as part of
; installer.cmake. It will not work on its own.
;
; Inno Setup 6 or later is required for this script to work.

[Setup]
AppID=FlexASIO
AppName=FlexASIO
AppVerName=FlexASIO @FLEXASIO_VERSION@
AppVersion=@FLEXASIO_VERSION@
AppPublisher=Etienne Dechamps
AppPublisherURL=https://github.com/dechamps/FlexASIO
AppSupportURL=https://github.com/dechamps/FlexASIO/issues
AppUpdatesURL=https://github.com/dechamps/FlexASIO/releases
AppReadmeFile=https://github.com/dechamps/FlexASIO/blob/@DECHAMPS_CMAKEUTILS_GIT_DESCRIPTION@/README.md
AppContact=etienne@edechamps.fr
WizardStyle=modern

DefaultDirName={autopf}\FlexASIO
AppendDefaultDirName=no
ArchitecturesInstallIn64BitMode=x64

[Files]
Source:"install\x64-Release\bin\FlexASIO.dll"; DestDir: "{app}\x64"; Flags: ignoreversion regserver 64bit; Check: Is64BitInstallMode
Source:"install\x64-Release\bin\*"; DestDir: "{app}\x64"; Flags: ignoreversion 64bit; Check: Is64BitInstallMode
Source:"install\x86-Release\bin\FlexASIO.dll"; DestDir: "{app}\x86"; Flags: ignoreversion regserver
Source:"install\x86-Release\bin\*"; DestDir: "{app}\x86"; Flags: ignoreversion
Source:"..\..\*.txt"; DestDir:"{app}"; Flags: ignoreversion
Source:"..\..\*.md"; DestDir:"{app}"; Flags: ignoreversion
Source:"..\..\*.jpg"; DestDir:"{app}"; Flags: ignoreversion

[Run]
Filename:"https://github.com/dechamps/FlexASIO/blob/@DECHAMPS_CMAKEUTILS_GIT_DESCRIPTION@/README.md"; Description:"Open README"; Flags: postinstall shellexec nowait skipifsilent
