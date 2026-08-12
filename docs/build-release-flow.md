# MiMITA Build & Release Flow

Last updated: 2026-08-11

This doc explains the real build pipeline, the canonical release folder, where
the version comes from, and how to use `devscripts/mimita-build-menu.py`.

---

## 1. Current build pipeline (before the menu)

All builds already existed. `mimita-build-menu.py` only orchestrates them; it
does not change how any of them compile.

| Step | Input | Command | Output |
|---|---|---|---|
| Debug game build | `src/**`, `src/pch.h`, `src/glad.c`, `external/libjuice/src/*.c`, `mimita.rc` | `python build_agent.py` (calls `build.py build-only`, mode `debug`, flags `-Og -g1`) | `mimita.exe` at repo root, objects in `build/obj-debug/` |
| Release game build | same sources | `python build_agent.py release` (flags `-O2 -march=x86-64-v2 -s -DNDEBUG`) | `mimita.exe` at repo root, objects in `build/obj-release/` |
| Launcher build | `launcher/main.cpp`, `launcher/launcher.rc`, `launcher.manifest`, `launcher/launcher-gui.json`, `external/miniz/*.c` | `cmd /c launcher\build.bat` | `MimitaLauncher.exe` at repo root |
| Zip bundle | root `mimita.exe` + `version.txt` + `glfw3.dll`, `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll` + `assets/ config/ shaders/ Characters/` (minus exclusions) | `python devscripts/bundle-game.py` | `mimita-game.zip` at repo root |

Key facts:

- Debug and release both link to the **same** `mimita.exe` at the repo root.
  The only difference is the object dir (`build/obj-debug/` vs `build/obj-release/`)
  and the compiler flags. A debug `mimita.exe` is ~90 MB; a release one is ~14 MB.
  `bundle-game.py` refuses to bundle an exe larger than 60 MB because that is a
  debug build.
- `MimitaLauncher.exe` is **not** inside the zip. The zip contains `mimita.exe`,
  `version.txt`, the runtime DLLs, and the asset/config/shaders/Characters
  folders at the zip root. The launcher downloads the zip from GitHub, extracts
  it into `%LOCALAPPDATA%\MiMITA\versions\v<tag>\`, verifies the zip SHA-256,
  and keeps only the last two versions.
- Nothing in this list is uploaded automatically by `mimita-build-menu.py`.

## 2. Launcher / website download expectations

- GitHub release: repo `jorj1357/mimita-public`, tag `v<version>` (e.g. `v2.0.1`),
  with three assets: `mimita-game.zip`, `MimitaLauncher.exe`, `launcher_info.json`.
- `launcher_info.json` carries `launcher_version`, `game_version`, `game_zip_url`,
  `game_zip_sha256`, `launcher_url`, `launcher_sha256`, `changelog`. The launcher
  reads it (falling back to the release tag + release body if absent).
- The website `/api/download/latest` redirects to
  `https://github.com/jorj1357/mimita-public/releases/latest/download/MimitaLauncher.exe`.

## 3. Canonical release folder

```
release/<version>/
  mimita.exe            (release build, ~14 MB)
  MimitaLauncher.exe
  mimita-game.zip
  release-hashes.txt    (SHA-256 + size of the three artifacts)
  av-scan-report.txt    (Defender scan results)
  launcher_info.json    (real GitHub URLs, ready to upload as the 3rd asset)
```

Currently `<version>` resolves to `2.0.1`, so the folder is
`release/2.0.1/`. The repo root keeps the raw build outputs
(`mimita.exe`, `MimitaLauncher.exe`, `mimita-game.zip`); `release/<version>/`
is the clean, verified, copy that is ready to upload.

## 4. Where the version comes from

`version.txt` at the repo root (currently `2.0.1`). It is:

- generated from `config/version.json` by `python devscripts/generate-version.py`,
- git-tracked, and
- the exact file `bundle-game.py` and `publish-release.py` already read.

`mimita.rc` also hardcodes the VERSIONINFO fields for `mimita.exe`; keep those
in sync manually when bumping a version. The menu treats `version.txt` as the
single source of truth for the release folder name and `launcher_info.json`.

## 5. Using the build menu

```
python devscripts/mimita-build-menu.py
```

| Choice | What it does |
|---|---|
| 1 = Debug build + launch | `python build_agent.py`, verify `mimita.exe`, then launch it (blocking; closes when you close the game). `--no-launch` skips the launch. |
| 2 = Release (quick clean) | delete `build/obj-release`, release build, launcher build, zip bundle, copy the 3 artifacts into `release/<version>/`. |
| 3 = Full clean release | also delete `src/pch.h.gch` and any stale `mimita-game.zip`, then string-scan the release exe for dev-path leaks, Defender-scan all 3 artifacts, and write `release-hashes.txt` + `av-scan-report.txt`. Slow (~430-file recompile). |
| 4 = Scan release | Defender + string scan + SHA-256 on whatever is staged in `release/<version>/`. |
| 5 = Full release prep | everything in option 3 plus `launcher_info.json`, then prints the exact upload command. |

Flags:

- `python devscripts/mimita-build-menu.py 2` runs that option directly (no menu).
- `--yes` skips confirmation prompts.
- `--no-launch` skips launching the game in option 1.

Behavior guarantees:

- prints every command before running it,
- stops immediately on a failed command,
- verifies each expected output exists,
- aborts if another `build_agent.py` build is running (lock file check),
- uses repo-relative paths, so it works from any checkout.

## 6. What the menu does NOT do

It does not upload, publish, `git tag`, commit, push, sign, or modify Defender
settings, and it never adds AV exclusions or bypasses. The final upload is a
manual step:

```
python devscripts/publish-release.py
```

or, uploading the staged files directly:

```
gh release create v2.0.1 release/2.0.1/mimita-game.zip release/2.0.1/MimitaLauncher.exe release/2.0.1/launcher_info.json --repo jorj1357/mimita-public --title "MiMITA v2.0.1" --notes "MiMITA v2.0.1 release."
```
