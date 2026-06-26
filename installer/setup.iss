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
Source: "..\assets.pak"; DestDir: "{app}"; Flags: ignoreversion

; Shaders
Source: "..\shaders\basic.vert"; DestDir: "{app}\shaders"; Flags: ignoreversion
Source: "..\shaders\basic.frag"; DestDir: "{app}\shaders"; Flags: ignoreversion
Source: "..\shaders\post.vert"; DestDir: "{app}\shaders"; Flags: ignoreversion
Source: "..\shaders\post.frag"; DestDir: "{app}\shaders"; Flags: ignoreversion
Source: "..\shaders\shadow.vert"; DestDir: "{app}\shaders"; Flags: ignoreversion
Source: "..\shaders\shadow.frag"; DestDir: "{app}\shaders"; Flags: ignoreversion

; Characters
Source: "..\Characters\DefaultGuy\manifest.json"; DestDir: "{app}\Characters\DefaultGuy"; Flags: ignoreversion
Source: "..\Characters\DefaultGuy\character.glb"; DestDir: "{app}\Characters\DefaultGuy"; Flags: ignoreversion

; Config files
Source: "..\config\accounts\default.json"; DestDir: "{app}\config\accounts"; Flags: ignoreversion
Source: "..\config\current-profile.json"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "..\config\analytics.json"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "..\config\collision.json"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "..\config\crosshair.json"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "..\config\hitfx.json"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "..\config\lighting.json"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "..\config\player-procedural.json"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "..\config\postfx.json"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "..\config\profiles.json"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "..\config\shadows.json"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "..\config\video-settings.json"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "..\config\version.json"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "..\config\dev_controls.txt"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "..\config\audio\hitmarker.json"; DestDir: "{app}\config\audio"; Flags: ignoreversion
Source: "..\config\audio\music-settings.json"; DestDir: "{app}\config\audio"; Flags: ignoreversion
Source: "..\config\audio\replay-hitmarkers.json"; DestDir: "{app}\config\audio"; Flags: ignoreversion
Source: "..\config\debug\debug-settings.json"; DestDir: "{app}\config\debug"; Flags: ignoreversion
Source: "..\config\effects\death-impact-ellipsoid.json"; DestDir: "{app}\config\effects"; Flags: ignoreversion
Source: "..\config\effects\spawn-fx.json"; DestDir: "{app}\config\effects"; Flags: ignoreversion
Source: "..\config\gameplay\void-death.json"; DestDir: "{app}\config\gameplay"; Flags: ignoreversion
Source: "..\config\gui\avatar-creator.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\bomb-tag-config.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\bomb-tag-hud.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\client-hud.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\community-menu.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\debug-menu.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\dev-overlay.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\duel-config-menu.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\duel-hud.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\duel-match-end.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\graphics-menu.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\help-menu.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\hud.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\input-debug.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\main-menu.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\music-widget.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\play-menu.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\practice-menu.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\replay-menu.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\sandbox-map-menu.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\server-info-menu.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\settings-menu.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\gui\sign-in-menu.json"; DestDir: "{app}\config\gui"; Flags: ignoreversion
Source: "..\config\replay\replay-export.json"; DestDir: "{app}\config\replay"; Flags: ignoreversion
Source: "..\config\video\outro.json"; DestDir: "{app}\config\video"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppLauncherName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppLauncherName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppLauncherName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
