; Mimita Windows Installer
; Inno Setup Script (Inno Setup 6)

#define MyAppName "Mimita"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Mimita"
#define MyAppURL "https://mimita.fun"
#define MyAppLauncherName "MimitaLauncher.exe"
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
Source: "..\MimitaLauncher.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\mimita.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\version.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\glfw3.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\libgcc_s_seh-1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\libstdc++-6.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\libwinpthread-1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\assets\*"; DestDir: "{app}\assets"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\config\accounts\default.json"; DestDir: "{app}\config\accounts"; Flags: ignoreversion
Source: "..\config\current-profile.json"; DestDir: "{app}\config"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppLauncherName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppLauncherName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppLauncherName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
