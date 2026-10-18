# Melee hitbox live path and actor damage

- Recorded at: 2026-10-18T17:12:37Z (America/New_York display context: 2026-10-18 13:12:37 EDT)
- Branch: `20261011zt`
- Base commit: `3a89b2de5059eee6256b5b4a614a83496acc5709`
- Status: partial implementation; runtime visual and multiplayer acceptance remain open.

## Pre-existing work preserved

The checkout already contained multiple unrelated edits and ongoing Melee work, including modifications to `config/weapons.json`, the Melee specification, server Melee actions, animation assets, movement/server configuration, dev-loop scripts/docs, and several changelogs/regression files. These were preserved. In `config/weapons.json`, `a_ground_punch.double_click_window_ticks` was already changed from `30` to `5`, and its base hitbox `active_end_tick` was already changed from `2` to `22`; neither edit was authored in this session. The parser still clamps the latter to the configured three-tick active phase, so the `22` value does not currently extend the active phase.

The existing uncommitted Melee section 58 and the earlier shared-hitbox changelog were also present before this session. This entry records the additional clarification and implementation below; it does not claim section 58 was newly written here.

## Changes made

- `docs/specs/gamemodes/supersmashbrosmelee.md:1-3`: replaced the opening raw TODO marker. Before: `2026 10 08 1228 jorj todo explainbetter`. After: `The opening informal notes below are historical context from early design discussion. The numbered sections that follow contain the current requirements; when a note and a numbered requirement differ, the numbered requirement is authoritative.`
- `docs/specs/gamemodes/supersmashbrosmelee.md` (section 58): clarified the per-tick JSON override contract. Before: hitbox data was required to be editable at each active tick, without defining the override structure. After: base hitbox values with optional `per_tick` entries keyed by active-phase-local `tick`; each entry overrides only provided limb, shape, size, or `local_offset` fields.
- `src/combat/weapon-json-config.cpp:428-438`, `registerWeaponsFromJsonRoot`: before, already-registered definitions were skipped. After, existing definitions still retain their C++ base definition, while a configured Melee component is parsed and republished to `MeleeMoveSetRegistry` on reload. This fixes edits to already-registered Captain Falcon move-set data being ignored.
- `src/combat/weapon-melee-config.h:50-81`: added `MeleeHitboxTick` and `resolveMeleeHitboxForTick` to apply a tick-specific limb/shape/size/offset over base hitbox values.
- `src/combat/weapon-melee-config.cpp:112-138`: parses optional `per_tick` records, inheriting the base values when an override omits a field.
- `src/network/server-melee-actions.cpp:648-654, 790-798`: resolves the current active-tick hitbox for both player- and NPC-originated attacks before calculating the existing server contact geometry.
- `src/network/server-melee-actions.cpp:709-742`: player attacks on NPCs now add Melee percent and scale knockback by the NPC's existing percent, and include percent values in the confirmed-hit event. Before, this branch applied knockback only and explicitly left NPC percent untracked.
- `src/combat/melee-client.h:55`, `src/combat/melee-client.cpp:253-279, 439-440`, and `src/engine/engine-tick-render.cpp:527`: added a translucent red sphere rendered from the local player's animated attached-part transform during configured active ticks; it uses the same tick resolver as server contact.

## Validation and evidence

- Routed documents reviewed: `docs/ROUTER.md`; Melee, weapons, networking, NPC behavior, animation, JSON configuration, logging, runtime scenario, build/live-development, and task-completion documents selected by the router. Focused reviews: `docs/skills/spec-behavior-review-v1.md` and `docs/skills/logging-checker-v1.md`.
- `config/weapons.json` parsed as valid JSON.
- `git diff --check` passed for the source files changed here. The full changed-spec check reports trailing spaces in the pre-existing informal notes at lines 805-826; these were not rewritten.
- Canonical dev-loop build 2593 succeeded, with 4 translation units compiled and 551 skipped. The exact executable was `.dev/builds/2593/mimita.exe`; its `--versioninfo` output reported `EVENTS_JSONL_PATH=logs/10-18-2026/20261018_131236/events.jsonl` and run ID `20261018_131236`.
- No real attack scenario was run and no hitbox visual was accepted by a human. The dev-loop still showed a running build-2590 client/server run while build 2593 was published; that run was left intact. The version-info process alone is not runtime gameplay evidence.

## Remaining gaps

The visual currently covers only the local player's sphere hitboxes. Remote-player and NPC attack visualization still needs authoritative action/active-tick state to reach observing clients. Server contacts still use root-relative part-height approximations rather than the animated limb transforms used by the local visual. The contact path currently forces configured shapes to spheres, and the NPC result change duplicates the player force calculation; a fully shared Force Punch contact/force owner remains outstanding. The `a_ground_punch.active_end_tick: 22` value is clamped to ticks `0..2` while its active phase has three ticks. These gaps prevent claiming that every attack, actor, shape, and observer path now matches section 58.
