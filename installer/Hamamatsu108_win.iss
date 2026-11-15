; Kuprell Hamamatsu 108 - Windows installer (Inno Setup 6)
#define AppName "Hamamatsu 108"
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#define Art "..\build\plugins\hamamatsu108\Hamamatsu108_artefacts\Release"

[Setup]
AppId={{6E1B8F43-2C7A-4D19-9B5E-108A81C4D0B2}
AppName=Kuprell {#AppName}
AppVersion={#AppVersion}
AppPublisher=Kuprell
DefaultDirName={commonpf64}\Kuprell\Hamamatsu 108
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputBaseFilename=Kuprell_Hamamatsu108_{#AppVersion}_Windows
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
; close FL Studio / any host that holds the plugin open, and replace locked files on reboot as a last resort
CloseApplications=force
CloseApplicationsFilter=*.exe,*.dll,*.vst3
RestartApplications=no

[Components]
Name: "vst3"; Description: "VST3 plugin (FL Studio, Ableton, Cubase, Studio One...)"; Types: full compact custom; Flags: fixed
Name: "standalone"; Description: "Standalone app"; Types: full

[Files]
Source: "{#Art}\VST3\Hamamatsu 108.vst3\*"; DestDir: "{commoncf64}\VST3\Hamamatsu 108.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs restartreplace uninsrestartdelete
Source: "{#Art}\Standalone\Hamamatsu 108.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion restartreplace

[Icons]
Name: "{commonprograms}\Kuprell Hamamatsu 108"; Filename: "{app}\Hamamatsu 108.exe"; Components: standalone
