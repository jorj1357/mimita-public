# NPC difficulty heavy movement preset

- EST timestamp: 2026-09-28 20:05
- Branch: `afad20a-rebuild`
- Result: PASS_WITH_HUMAN_REVIEW

## Change

Changed `config/npc-difficulty.json:39` from:

```json
"movementPreset": "follow"
```

to:

```json
"movementPreset": "heavy"
```

NPCs now resolve `config/movement/movement-heavy.json` instead of following
the player's active movement preset.

## Existing loader proof

No new movement loader was added because the existing owner already satisfies
the requested design:

- `src/config/movement-config.cpp:471-503` scans every regular `.json` file in
  `config/movement/`, matches its JSON `name`, and falls back to
  `movement-<preset>.json`.
- `src/npc/npc-difficulty-config.cpp:177-203` calls
  `MovementJsonConfig::loadPresetInto()` for any NPC `movementPreset` other
  than `follow`.
- `src/npc/npc.cpp:898-902` uses the resolved NPC preset for navigation and
  movement decisions.
- The existing reload path re-reads the NPC difficulty file and its selected
  movement preset while the game is running.

The pre-existing edits to `config/movement.json` and
`config/movement/movement-heavy.json` were preserved; they were not created or
rewritten by this session.

## Validation

- Confirmed `movement-heavy.json` contains `"name": "heavy"`.
- Confirmed the loader scans all `.json` files under `config/movement/`.
- Confirmed `git diff --check` reports only pre-existing trailing whitespace
  in `config/movement/movement-heavy.json`.
- No C++ source changed, so a rebuild was not required for this configuration-
  only hot-reload change.

## Human/runtime acceptance still required

With the updated running session, verify the NPC movement log identifies
`movement-heavy.json`, then compare an NPC's movement against the player and
edit `movementPreset` to another preset to confirm the NPC changes live.

