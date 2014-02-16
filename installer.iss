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
Source:"Release\ASIOBridge.dll"; DestDir: "{app}"; Flags: ignoreversion regserver
Source:"portaudio_x86.dll"; DestDir: "{app}"; Flags: ignoreversion
Source:"LICENSE.txt"; DestDir:"{app}"; Flags: ignoreversion
