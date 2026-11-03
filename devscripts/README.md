# Development Scripts

**Purpose:** show contributors where repository maintenance and validation
scripts live.
**Audience:** MiMITA contributors and maintainers.
**Does not:** define runtime game features, creator workflows, or production
deployment.

## Current groups

- Development loop and launch: `dev-loop.py`, `dev-loop-watch.py`,
  `dev-loop-matrix.py`, `dev-profiles/`, and the `dev_loop/` package.
- Dev-loop components: `dev_loop/cli.py` owns the stable flags and aliases;
  `dev_loop/artifacts.py` owns UTC IDs, safe folder names, artifact discovery,
  and sidecar identity; `dev_loop/build.py`, `dev_loop/launch.py`,
  `dev_loop/state.py`, and `dev_loop/runtime.py` own build, launch selection,
  durable state, and executable/runtime identity. `dev_loop/status.py` owns
  shared status formatting, `dev_loop/gui.py` owns the Tk dashboard, and
  `dev_loop/terminal.py` owns terminal setup. The remaining daemon lifecycle,
  control, status aggregation, scenario, and matrix owners are still in
  `dev-loop.py` and should be extracted in small audited steps. Focused
  artifact identity and fallback checks live in `dev_loop/test_artifacts.py`.
  `MIMITA_DEV_LOOP_GUI_TEST_ACTION=launch_latest` invokes the same Launch
  button callback for a dashboard control-path smoke check.
- Scenario data and examples: `scenarios/`, including `matrix/` and
  `user/`; scenario execution and reports: `scenario-analyze.py` and
  `scenario_replay_report.py`.
- Quality and evidence: `quality/`, `perf-watch.py`, and
  `perf-evidence.py`.
- Configuration and content validation: `jsonc.py`, `config_selftest.py`,
  `animation_json_check.py`, `gamemode_json_sync.py`, and
  `weapon_json_sync.py`.
- Asset/development integrations and generation: `blender-ai/`,
  `blender/`, `generate_zombie_tower_slice.py`, and
  `migrate_avatar_animation_profiles.py`.
- Packaging and platform setup: `package-release.py` and
  `build-minimal-ffmpeg.sh`.

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

For existing top-level helpers, use this sequence: inventory references,
move one responsibility group, leave a shim at every old path, update the
inventory, and verify both the old invocation and the new package invocation.
Do not move scenario data or profiles into Python packages.

## ClipForge

The user-provided ClipForge CMD folder is an input to the
[In-App Video Editor](../docs/specs/creator-tools/video-editor.md) design and
should be audited before porting. Its player-facing editing and rendering
features belong in the in-game creator workspace. Repository maintenance
scripts do not become a substitute video editor, and account credentials or
browser profiles must not be copied into this repository.
