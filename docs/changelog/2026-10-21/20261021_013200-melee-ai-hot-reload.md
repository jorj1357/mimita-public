# Melee NPC attack tuning and remote sounds

- Status: `PASS_WITH_HUMAN_REVIEW`. The Melee spec and implementation now use a hot-reloadable behavior profile for NPC attack timing, range, and move weights. Remote action snapshots trigger each move's configured start and phase sounds. The build succeeded; in-match cadence, hot reload, and hearing the sounds still need a Melee runtime session and human review.
- Time: 2026-10-21T01:32:00Z (2026-10-20 21:32:00 EDT, America/New_York).
- Branch: `20261011zt`.
- HEAD at start: `f6d6eeb2`.
- Pre-existing working-tree edits: many unrelated configuration, source, and spec edits were present. In overlapping files, preserved the Sandbox role's pre-existing movement change in `config/roles.json`, the player action-snapshot and animation work in `src/network/multiplayer-interpolation.cpp`, and the equipped-weapon/animation-only work in `src/network/server-melee-actions.cpp`.

## Implementation

- Added `melee_aggressive` in `config/behavior-profiles.json`, selected for NPCs using the `falcon` role. Its initial values are a one-tick gap after a move ends, a 4.5 m range, and weighted quick moves with rare specials. `melee_attack_gap_ticks`, `melee_attack_range_meters`, and `melee_move_weights` are parsed by the already-hot-reloaded behavior-profile registry; the server reads the current profile when choosing the next action. Move weights are filtered by the move's grounded/airborne requirement. Edits affect future choices, while an active move remains unchanged.
- Added profile name, target distance, selected range, attack gap, and move weight to the existing `melee.action.start` event for runtime diagnosis.
- Added remote action-snapshot audio playback using each move's existing sound data in `config/weapons.json`. Start sounds play on action change. Phase sounds fire once as replicated elapsed ticks cross their authored event ticks, including tick 0.
- Updated section 34 of the Melee spec with the settings' exact file/field names, defaults, reload behavior, actor-preset versus behavior-profile ownership, and remote audio behavior.

## Exact file edits

- `docs/specs/gamemodes/supersmashbrosmelee.md:237,239`: replaced the unresolved NPC decision-policy note with the decided hot-reload profile contract and added the remote move-sound contract. Before, section 34 ended `**OPEN:** NPC decision policy and how reusable moves map into non-Melee modes.` After, it only leaves move reuse in other modes open and specifies the Melee profile and sound behavior.
- `config/behavior-profiles.json:666-681`: new profile; there was no prior `melee_aggressive` entry. It defines the one-tick gap, 4.5 m range, quick-action weights of 10/6, and special weights of 1.
- `config/roles.json:146`: changed the Falcon NPC role profile. Before: `"behavior_profile": "balanced",`; after: `"behavior_profile": "melee_aggressive",`. The earlier unrelated Sandbox movement edit in this file is preserved.
- `src/npc/npc-behavior.h:41-43`: added profile fields. Before the weapon-switch field, the profile struct proceeded directly to pursuit settings. After: `int meleeAttackGapTicks = -1;`, `float meleeAttackRangeMeters = -1.0f;`, and `std::unordered_map<std::string, float> meleeMoveWeights;`.
- `src/npc/npc-behavior.cpp:75-94`: added tolerant gap/range/weight parsing. Before, parsing proceeded from `weapon_switch_threshold` directly to `pursuit_mode`; after it reads the three Melee fields, leaving wrong-typed or missing values at the compatibility defaults.
- `src/network/server-melee-actions.cpp:665-694,1146-1204`: replaced the fixed 20-tick post-move delay and hard-coded occasional special choice with resolved profile timing/range/weighted move choice; added grounded/airborne filtering and selection fields to the existing action-start event. Before, NPCs waited 20 ticks and used a fixed every-fifth special rule. After, profile values control those choices.
- `src/entities/player.h:529-532`: added per-replica action/audio tick deduplication state. Before, the network serial fields went directly from equip serial to network state flags; after, action start tick, action elapsed tick, action index, and initialized state sit between them.
- `src/network/multiplayer-interpolation.cpp:1709-1747`: added remote action lookup and world-positioned start/phase sounds. Before, this block only selected the animation clip; after, it deduplicates the sound events per action and plays configured move sounds as snapshot ticks advance.
- This changelog is new; there was no prior text.

## Documents and focused review

- Read `AGENTS.md`, `docs/ROUTER.md`, the Melee, NPC behavior, actor preset, networking, JSON configuration, and debug logging specifications/architecture; the runtime-scenario, live-development, terminal, time-formatting, build, and task-completion workflows; and `docs/skills/spec-behavior-review-v1.md`, `docs/skills/logging-checker-v1.md`, and `docs/skills/documentation-checker-v1.md`.
- The router-linked `docs/doc-review-09-03-2026.md` and `docs/regressions/regressions-v1.md` are absent in this checkout.
- Focused spec/config/source review: `PASS`. Profile selection follows the NPC-only `behavior_profile` contract; the profile loader already polls this file, and the Melee decision reads its current value for future action starts.
- No new tests were added or run.

## Validation evidence

- `config/behavior-profiles.json` and `config/roles.json` parsed after removing their comment-only lines; the profile list contains 12 entries and the roles list contains 23.
- `git diff --check` passed for the eight implementation/spec/config files changed here. Git reports a line-ending conversion warning for the already-modified `src/network/server-melee-actions.cpp`; it did not report whitespace errors in these focused files.
- Canonical dev-loop build 2716 succeeded and published current sources. The build output reported 1 compiled translation unit and 563 skipped. The prior canonical builds compiled the changed Melee server, NPC profile parser, and remote interpolation units; the final build compiled the last server/interpolation changes.
- `.dev/builds/2716/mimita.exe --versioninfo` reported `EVENTS_JSONL_PATH=logs/10-20-2026/20261020_213140/events.jsonl`; executable SHA-256: `3DD9432860052113365E64D8F63E26670F426715F000D4B6496903515270F127`.
- No gameplay scenario was run. The existing dev-loop session is a long-running, visible launch with server/client build 2706, configured launch mode `juggernaut`, map `dust2cyberiav4`, and no-NPC runtime arguments; the latest executable is 2716. It was left untouched to avoid interrupting the user's session. Therefore this work does not claim that live profile edits were observed, the NPC attacked more often, remote sounds were heard, or NPC melee hits behaved correctly in play.
- Confirm Melee attack cadence and profile hot reload, weighted move distribution, NPC-to-player contact/sounds, and remote-player sound playback in a dedicated Melee run. The NPC death server crash and other previously reported Melee problems were not investigated or fixed by this change.
