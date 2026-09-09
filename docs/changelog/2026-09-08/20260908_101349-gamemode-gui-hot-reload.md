# Gamemode GUI hot reload migration

Timestamp: 2026-09-08T14:13:49Z (2026-09-08T10:13:49-04:00)
Branch: `8292026stash`
Commits: none created by this session
Status: PASS_WITH_HUMAN_REVIEW

## Pre-existing working-tree state

The following edits/untracked paths existed before this session and were not
claimed, changed, or reverted:

- `config/accounts/default.json`
- `config/analytics.json`
- `config/ragdoll.json`
- `docs/specs/ragdoll-retrograd/`

## Request and reason

The requested behavior was to make gamemode GUI presentation live-editable from
one JSON file selected by the gamemode ID, and to record the unresolved
regression where the GUI specification was stricter than the implementation.
Overlap detection was explicitly excluded.

## Exact source/config changes

### Shared layout owner

`src/gui/gui-layout.h:163-267` changed from a single-file `GuiLayout::load`
API and one `mLayouts` map to:

```cpp
bool load(const std::string& filePath, const std::string& sectionId);
GuiLayout& getGamemodeLayout(const std::string& gamemodeId);
std::unordered_map<std::string, GuiLayout> mGamemodeLayouts;
```

`src/gui/gui-layout.cpp:1-13` now has the required source ownership header.
`src/gui/gui-layout.cpp:90-103` preserves the old load API by delegating to the
new overload. `src/gui/gui-layout.cpp:253-265` selects and validates
`gamemodes.<id>` before parsing its `elements` object. The old path was only a
top-level `elements` parser; the new path accepts the gamemode metadata file
without creating a second element parser.

`src/gui/gui-layout.cpp:512-528` adds normalization:

```cpp
team_deathmatch -> tdm
free_for_all    -> ffa
bomb_tag        -> bombtag
```

`src/gui/gui-layout.cpp:551-561` polls already-loaded gamemode sections from
`config/gui/gamemode-meta-gui.json` and preserves the prior valid section on a
failed reload.

### Gamemode presentation source

`config/gui/gamemode-meta-gui.json:1-86` is new. It contains sections for the
exact active gamemode IDs found in `config/gamemodes/*.json`:

```text
tdm, ffa, bombtag, duel, sandbox
```

The top-level `_comment` records that string IDs remain authoritative and that
numeric IDs are only a future registry decision.

The `tdm` section owns title, central score, red score, blue score, team
indicator, time-left text, countdown, intermission, score gain, and recording
indicator presentation values.

### Consumers migrated

- `src/engine/engine-tick-ui-overlays.cpp:557-563` now selects the metadata
  section from the replicated match mode.
- `src/gui/hud/match-leaderboard.cpp:39,53,83` now reads the active mode's
  metadata section instead of `config/gui/match-hud.json`.
- `src/game/gamemode-manager.cpp:267` now reads Bomb Tag presentation from
  the `bombtag` metadata section instead of `config/gui/bomb-tag-hud.json`.
- `src/duel/duel-ui.cpp:192` and `src/game/duel.cpp:241` now use the `duel`
  metadata section instead of their separate duel HUD files.

### Documentation and regression record

- `docs/specs/hotreload/hotreload.md:1-123` documents the current master gate,
  polling path, reload cadence, parser, last-known-good behavior, terminal
  commands, gamemode metadata path, and remaining hardcoded presentation.
- `docs/regressions/regressions-v1.md` received one append-only entry at the
  end: `2026-09-08T14:12:27Z — Gamemode GUI presentation is not uniformly JSON
  hot-reloadable (UNRESOLVED)`. It records exact pre-migration paths, the
  specification disagreement, the migration, and the requirement for live
  runtime proof before resolving the regression.

## Validation

- Parsed `config/gui/gamemode-meta-gui.json` successfully.
- Confirmed metadata coverage exactly matches the five gamemode IDs from
  `config/gamemodes/*.json`.
- `git diff --check`: passed; only normal LF-to-CRLF warnings were reported by
  Git for edited files.
- Canonical build: `python build_agent.py` ultimately passed with
  `Status: SUCCESS`, `Return Code: 0`, and `mimita.exe` relinked. The first
  build attempt reached compilation but failed because two existing
  `mimita.exe` processes held the output; those processes were stopped under
  the repository's explicit build-lock authorization. A retry briefly hit a
  runtime DLL lock, and the next retry succeeded.
- Focused review documents used:
  - `docs/skills/spec-behavior-review-v1.md`
  - `docs/skills/documentation-checker-v1.md`
  - `docs/operations/task-completion/task-completion.md`
  - `docs/specs/gui/guiv2.md`
  - `docs/architecture/json-configuration/json-configuration.md`
  - `docs/regressions/regressions-v1.md`

## Remaining human review

Live runtime testing is still required. While the source and build prove that
the migrated consumers use the new owner, they do not prove that saving edits
to the metadata file visibly updates every TDM, FFA, Bomb Tag, and duel HUD
without restart. Room status, reconnect suffix text, replay recording UI,
duel result text, and various debug overlays remain documented unresolved
hardcoded/partial paths. Overlap detection was not implemented.
