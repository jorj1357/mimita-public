; Mimita Windows Installer
; Inno Setup Script (Inno Setup 6)

#define MyAppName "Mimita"
#define MyAppVersion "2.0.0"
#define MyAppPublisher "Mimita"
#define MyAppURL "https://mimita.fun"
#define MyAppExeName "mimita.exe"

[Setup]
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
DefaultDirName={localappdata}\Mimita
DefaultGroupName=Mimita
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=.
OutputBaseFilename=MimitaSetup-{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
DisableWelcomePage=yes
DisableDirPage=auto
DisableReadyPage=yes
CloseApplications=no
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: checkedonce

[Files]
Source: "..\mimita.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\version.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\glfw3.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\libgcc_s_seh-1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\libstdc++-6.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\libwinpthread-1.dll"; DestDir: "{app}"; Flags: ignoreversion

; Assets (all loose files — no more assets.pak)
Source: "..\assets\*"; DestDir: "{app}\assets"; Flags: ignoreversion recursesubdirs createallsubdirs

; Shaders
Source: "..\shaders\*"; DestDir: "{app}\shaders"; Flags: ignoreversion recursesubdirs createallsubdirs

; Characters
Source: "..\Characters\*"; DestDir: "{app}\Characters"; Flags: ignoreversion recursesubdirs createallsubdirs

; Config (all files — auto-discovered)
Source: "..\config\*"; DestDir: "{app}\config"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\assets\uitextures\mimita desktop icon v1.ico"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon; IconFilename: "{app}\assets\uitextures\mimita desktop icon v1.ico"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent

[Registry]
Root: HKCU; Subkey: "Software\Classes\mimita"; ValueType: string; ValueName: ""; ValueData: "URL:Mimita Protocol"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\mimita"; ValueType: string; ValueName: "URL Protocol"; ValueData: ""; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\mimita\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\mimita\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey

[UninstallRun]
