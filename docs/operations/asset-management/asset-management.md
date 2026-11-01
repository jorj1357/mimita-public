# Asset Rules

* Gameplay sound effects under `assets/sound/entity/`, `assets/sound/ui/`, `assets/sound/weapon/` ARE tracked in Git.
* Music production files under `assets/music/` and loose source/production `.wav`/`.mp3` files are IGNORED by `.gitignore` and must NEVER be committed.
* Game loads music from `assets/music/` at runtime.
* If a game feature needs new gameplay SFX, add the `.wav` or `.mp3` file to `assets/sound/entity/`, `assets/sound/ui/`, or `assets/sound/weapon/` and `git add -f` it (the global ignore allows explicit tracking of gameplay audio).
* Do not create, modify, or delete music production files.

# Asset Management

* `assets/music/ingame/donttrack` is intentionally excluded from source control via `.gitignore`. It contains local music work files, exports, experiments, and temporary audio assets. Any file that should become part of the game must be moved out of this folder into `assets/music/ingame/`.

## Bundled FFmpeg

`tools/ffmpeg.exe` is tracked because it is the packaged video/audio decoder.
It is a deliberately small static build, not a full FFmpeg distribution. Its
feature whitelist must keep the replay encoder plus the music paths:
`wav`, `mov/mp4/m4a`, `matroska`, `ogg`, `opus`, `vorbis`, `aac`, and PCM,
with the `wav` and `null` muxers used by cached music decoding and diagnostics.

Rebuild it with `devscripts/build-minimal-ffmpeg.sh` from an MSYS2 MINGW64
shell. Do not replace it with the full multi-hundred-megabyte system build.
When adding a new music container or codec, update the script whitelist,
rebuild, and verify the resulting binary can decode the actual files before
committing it. A binary that contains a codec decoder but lacks its container
demuxer is not sufficient.

# Reusable Blender-authored object assets

Reusable object assets live under `assets/objects/library/<asset-id>/`.
Each package has one stable asset ID and a manifest named `asset.json`.
The runtime model is exported as `model.glb`; an editable Blender source
file may be included as `source.blend` when the author intends to share it.
Textures and other required runtime files stay inside the package or use
existing tracked shared-asset paths.

The manifest identifies the asset, its origin/pivot, render model, collision
representation, default physical material and density, and any authored
attachment points. Maps store a reference to the stable asset ID plus an
instance transform and allowed per-instance overrides; they do not duplicate
the mesh into map JSON.

Blender export/import must preserve the expected units and axis convention,
report missing textures, and validate that collision metadata is usable before
the object is placed in a map. An asset package contains data only: importing
it must not execute scripts. Record author and license/provenance so creators
can tell whether they may reuse or redistribute it.

The existing `assets/objects/` tree remains the runtime object root for
weapons and game objects. The library is a new organized area beneath that
root, not a second asset loader. Map assets remain tracked under
`assets/maps/`.

## Creator source and generated content

Do not commit secrets, account credentials, browser profiles, user-local
exports, or temporary renders. User-created map content and video projects
remain user data unless explicitly packaged for distribution. Music production
files remain subject to the existing exclusion rules above.

---

