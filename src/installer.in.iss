[Setup]
AppID=FlexASIO
AppName=FlexASIO
AppVerName=FlexASIO @FLEXASIO_VERSION@
AppVersion=@FLEXASIO_VERSION@
AppPublisher=Etienne Dechamps
AppPublisherURL=https://github.com/dechamps/FlexASIO
AppSupportURL=https://github.com/dechamps/FlexASIO/issues
AppUpdatesURL=https://github.com/dechamps/FlexASIO/releases
AppReadmeFile=https://github.com/dechamps/FlexASIO/blob/@FLEXASIO_GITSTR@/README.md
AppContact=etienne@edechamps.fr

DefaultDirName={pf}\FlexASIO
AppendDefaultDirName=no
ArchitecturesInstallIn64BitMode=x64

[Files]
Source:"x64\install\bin\FlexASIO.dll"; DestDir: "{app}\x64"; Flags: ignoreversion regserver 64bit; Check: Is64BitInstallMode
Source:"x64\install\bin\*"; DestDir: "{app}\x64"; Flags: ignoreversion 64bit; Check: Is64BitInstallMode
Source:"x86\install\bin\FlexASIO.dll"; DestDir: "{app}\x86"; Flags: ignoreversion regserver
Source:"x86\install\bin\*"; DestDir: "{app}\x86"; Flags: ignoreversion
Source:"*.txt"; DestDir:"{app}"; Flags: ignoreversion
Source:"*.md"; DestDir:"{app}"; Flags: ignoreversion

[Run]
Filename:"https://github.com/dechamps/FlexASIO/blob/@FLEXASIO_GITSTR@/README.md"; Description:"Open README"; Flags: postinstall shellexec nowait
