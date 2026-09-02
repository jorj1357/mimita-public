# Asset Rules

* Gameplay sound effects under `assets/sound/entity/`, `assets/sound/ui/`, `assets/sound/weapon/` ARE tracked in Git.
* Music production files under `assets/sound/music/` and loose source/production `.wav`/`.mp3` files are IGNORED by `.gitignore` and must NEVER be committed.
* Game loads audio from `assets/sound/` at runtime.
* If a game feature needs new gameplay SFX, add the `.wav` or `.mp3` file to `assets/sound/entity/`, `assets/sound/ui/`, or `assets/sound/weapon/` and `git add -f` it (the global ignore allows explicit tracking of gameplay audio).
* Do not create, modify, or delete music production files.

# Asset Management

* `assets/sound/music/ingame/donttrack` is intentionally excluded from source control via `.gitignore`. It contains local music work files, exports, experiments, and temporary audio assets. Any file that should become part of the game must be moved out of this folder into the proper tracked asset location.

# assets that DO get tracked 

maps get tracked by git assets\maps\

9 2 2026 expand, because, we should add a folder of 

---

