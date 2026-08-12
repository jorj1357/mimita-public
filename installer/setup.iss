; Mimita Windows Installer
; Inno Setup Script (Inno Setup 6)
; Ships only the launcher — game files are downloaded from GitHub on first run.

#define MyAppName "Mimita"
#define MyAppVersion "2.0.2"
#define MyAppPublisher "Mimita"
#define MyAppURL "https://mimita.fun"
#define MyAppLauncherName "MimitaLauncher.exe"

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
UninstallDisplayIcon={app}\{#MyAppLauncherName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: checkedonce

[Files]
; Only ship the launcher — it downloads the game from GitHub on first run
Source: "..\MimitaLauncher.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\version.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppLauncherName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppLauncherName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppLauncherName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent

[Registry]
Root: HKCU; Subkey: "Software\Classes\mimita"; ValueType: string; ValueName: ""; ValueData: "URL:Mimita Protocol"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\mimita"; ValueType: string; ValueName: "URL Protocol"; ValueData: ""; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\mimita\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppLauncherName},0"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\mimita\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppLauncherName}"" ""%1"""; Flags: uninsdeletekey

[UninstallRun]
