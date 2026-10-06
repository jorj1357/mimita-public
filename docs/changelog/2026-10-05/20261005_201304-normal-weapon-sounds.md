# Normal weapon sound retrigger and pitch

- EST timestamp: 2026-10-05 20:13:04 -04:00.
- Branch: `afad20a-rebuild`.
- Result: `PASS_WITH_HUMAN_REVIEW`.
- Pre-existing edits: the worktree already contained unrelated modifications in `config/accounts/default.json`, `config/analytics.json`, `config/behavior-profiles.json`, `config/gamemodes/sandbox.json`, `config/npc-difficulty.json`, collision/world/network source files, and `tests/collision-aabb-tree-test.cpp`. They were preserved and not changed by this task.

## Request and final state

The normal `revolver` and normal `shotgun` entries in `config/weapons.json` now use retrigger sound behavior. Their fire sound pitch multipliers are `0.9` and `0.8`, respectively. The existing audio implementation applies the pitch multiplier through `ma_sound_set_pitch`, which changes playback rate and pitch together as requested.

## Exact change

File: `config/weapons.json`

- Lines 70-71, normal revolver `sound` block.
  - Old: no `pitch` or `retrigger` keys after `"equip": "weapon/revolver/revolverequip"`.
  - New: `"pitch": 0.9,` and `"retrigger": true`.
- Lines 241-242, normal shotgun `sound` block.
  - Old: no `pitch` or `retrigger` keys after `"equip": "weapon/shotgun/shotgunequip"`.
  - New: `"pitch": 0.8,` and `"retrigger": true`.

The separate `op_revolver` and `aa12` variants were intentionally left unchanged.

## Documents and focused review

- `AGENTS.md` and `docs/ROUTER.md` were followed.
- Relevant specification: `docs/specs/weapons/weapons.md`.
- Relevant asset guidance: `docs/operations/asset-management/asset-management.md`.
- Relevant audio architecture: `docs/architecture/live-development/hot-audio-contract.md` and `docs/features/live-code-development/live-code-development.md`.
- Focused skills: `docs/skills/spec-behavior-review-v1.md` — PASS for this configuration-only behavior change; `docs/skills/asset-checker-v1.md` — PASS for tracked path and existing sound assets/config ownership.

## Validation evidence

- `config/weapons.json` parsed successfully with PowerShell JSON parsing.
- Parsed values confirmed: `revolver.pitch = 0.9`, `revolver.retrigger = true`, `shotgun.pitch = 0.8`, `shotgun.retrigger = true`.
- `git diff --check` found no whitespace issue in the weapon change. It reported a pre-existing trailing-space warning at `config/behavior-profiles.json:185`.
- The source parser `applyWeaponSoundJson` already reads `pitch` and `retrigger`; `WeaponAudio::playShootSound` already forwards both to the audio event, and `src/audio/audio.cpp` applies pitch through `ma_sound_set_pitch`.
- No build was run because only active JSON configuration changed.

## Human review still needed

Fire the normal revolver and normal shotgun in-game and confirm each shot retriggers over an already-playing shot, with the revolver audibly at 90% and shotgun at 80% playback speed/pitch. No runtime or human audio acceptance was performed in this session.
