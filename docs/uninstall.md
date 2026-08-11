<!-- 08 11 2026, 15 58 -->
<!-- purpose
* Explains how to fully uninstall the MiMITA game, launcher and user data.
* Required by SignPath Foundation's code of conduct ("provide uninstallation").
* Does NOT describe reinstalling or repairing MiMITA.
* Does NOT cover removing accounts created on the website.
-->

# Uninstalling MiMITA

MiMITA is a portable application. It does **not** install anything into the
Windows registry or `Program Files`. Everything the launcher and game create
lives under `%LOCALAPPDATA%\MiMITA`. To remove MiMITA completely, follow the
steps below.

## Step 1 — Exit MiMITA

- Close the game window.
- Right-click the MiMITA tray icon and choose **Exit** (if the tray icon is
  running). Otherwise make sure no `mimita.exe` or `MimitaLauncher.exe` process
  is running. You can check in Task Manager.

## Step 2 — Remove shortcuts (optional)

If you chose to create them during installation, delete the MiMITA shortcuts:

- Start Menu: `MiMITA` under `%APPDATA%\Microsoft\Windows\Start Menu\Programs`
- Desktop: `MiMITA` / `MiMITA Launcher` shortcut

## Step 3 — Delete the MiMITA data folder

Open a File Explorer window, type `%LOCALAPPDATA%\MiMITA` into the address bar,
and delete the folder. It contains:

| Path | Contents |
|------|----------|
| `%LOCALAPPDATA%\MiMITA\launcher` | The launcher executable and its update copies |
| `%LOCALAPPDATA%\MiMITA\launcher-data` | Launcher logs and crash dumps |
| `%LOCALAPPDATA%\MiMITA\versions\v<ver>\` | Installed game versions (one folder per version) |
| `%LOCALAPPDATA%\MiMITA\active-version.txt` | Which game version is currently active |
| `%LOCALAPPDATA%\MiMITA\crashes\` | Crash reports |
| `%LOCALAPPDATA%\MiMITA\<version folder>\config`, `logs`, `replays` | Your settings, logs, and saved replays |

Deleting this folder removes the game, the launcher, and your local settings,
logs, and replays. If you want to keep replays or configs, back them up before
deleting.

## Step 4 — Verify removal

- Check that `%LOCALAPPDATA%\MiMITA` no longer exists.
- Confirm no `MimitaLauncher.exe` or `mimita.exe` appears in Task Manager.

## Notes

- No uninstaller entry is registered in **Settings > Apps** because MiMITA does
  not use the registry-based install system.
- Deleting the folder does **not** delete your MiMITA website account; manage
  or delete that account on https://mimita.fun.
- Analytics data deletion can be requested separately from the in-game settings
  menu (see `docs/privacy-policy.md`).
