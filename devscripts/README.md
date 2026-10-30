# Development Scripts

**Purpose:** show contributors where repository maintenance and validation
scripts live.
**Audience:** MiMITA contributors and maintainers.
**Does not:** define runtime game features, creator workflows, or production
deployment.

## Current groups

- Development loop and launch: `dev-loop.py`, `dev_loop_terminal.py`,
  `dev-loop-watch.py`, `dev-loop-matrix.py`, and `dev-profiles/`.
- Scenario data and examples: `scenarios/`, including `matrix/` and
  `user/`.
- Quality and evidence: `quality/`, `perf-watch.py`, and
  `perf-evidence.py`.
- Configuration and content validation: `jsonc.py`,
  `config_selftest.py`, and the animation, gamemode, and weapon JSON
  synchronization/checking scripts.
- Asset/development integrations: `blender-ai/` and
  `migrate_avatar_animation_profiles.py`.
- Packaging: `package-release.py`.

## Placement rule

Scripts that build, inspect, validate, migrate, launch, or package the
repository belong here. Features that players use—replay editing, video
creation, text-to-speech, music production, and map authoring—belong in the
MiMITA application and their runtime owners, not in this development folder.

Before moving an existing script into a subfolder, search its command
references, task profiles, CI jobs, docs, and user workflows; update all
references in the same change. Keep scripts where they are until that reference
audit is complete. Group future additions by responsibility instead of adding
more unrelated files at the root.

## ClipForge

The user-provided ClipForge CMD folder is an input to the
[In-App Video Editor](../docs/specs/creator-tools/video-editor.md) design and
should be audited before porting. Its player-facing editing and rendering
features belong in the in-game creator workspace. Repository maintenance
scripts do not become a substitute video editor, and account credentials or
browser profiles must not be copied into this repository.
