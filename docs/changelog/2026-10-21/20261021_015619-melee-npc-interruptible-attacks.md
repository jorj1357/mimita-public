# Melee NPC interruptible attacks

- Status: `PASS_WITH_HUMAN_REVIEW`. The server now starts the next Melee NPC move at the configured 1–30 tick start-to-start interval, interrupting a still-running action when a live target remains in attack range. It mirrors the new action identity and start tick into the existing NPC snapshot fields, which drive the shared client animation and per-move sound path. Build 2718 succeeded. A fresh runtime/human Melee check remains outstanding.
- Time: 2026-10-21T01:56:19Z (2026-10-20 21:56:19 EDT, America/New_York).
- Branch: `20261011zt`; HEAD at start: `094e6e5a`.
- Pre-existing working-tree edits were preserved, including edits to `config/analytics.json`, `config/behavior-profiles.json`, `config/weapons.json`, the Melee spec, `src/npc/npc-behavior.cpp`, and `src/npc/npc-state-machine.cpp`, plus the three prior Melee changelog records. This session changed only `src/network/server-melee-actions.cpp` and added this changelog.

## Implementation

- `startNpcMeleeAction` now sets `nextAttackTick = startTick + clamp(melee_attack_gap_ticks, 1, 30)` rather than waiting for the move's full authored duration. It also updates `ServerNpc.meleeActionActive`, `meleeActionId`, and `meleeActionStartTick`, which `makeNpcEntity` already sends through `serverWriteWeaponActionSnapshot`.
- `tickServerMeleeActions` now considers a new attack whenever its scheduled tick arrives, even if the prior action is still active. On a legal new start, the action snapshot is replaced and the new move starts at tick 0; the old move's remaining active-hit window is discarded and its hit-target set is cleared.
- Completing or invalidating an NPC action now clears the mirrored snapshot action fields. Death also clears them.
- The existing shared `updateRenderedReplica` path resolves each replicated action against the actor's equipped move set, starts its configured animation and sounds, and detects an interruption using action index plus start tick. No parallel audio path or new packet was added.
- `melee.action.start` now records the start-to-start interval and whether it interrupted an earlier move, including the old move ID and elapsed ticks.

## Exact source edit

- `src/network/server-melee-actions.cpp`, `startNpcMeleeAction` (lines 669–710): before, the scheduler used `tick + max(1, action.totalTicks()) + 1`, and the NPC's snapshot action fields were not updated in this function. After, it uses `tick + clamp(attackGapTicks, 1, 30)`, copies the new action state into `ServerNpc`, and records interruption state in the existing `melee.action.start` event.
- `src/network/server-melee-actions.cpp`, NPC action selection (line 1163): before: `if (ms && !st.active && tick >= st.nextAttackTick) {`; after: `if (ms && tick >= st.nextAttackTick) {`. Starting the new action replaces the old action snapshot.
- `src/network/server-melee-actions.cpp`, end/invalid/death branches (lines 1041–1078): before, those branches only cleared the side-map action state; after, they also clear the corresponding `ServerNpc` snapshot action fields.
- This changelog is new; there was no prior text.

## Documents and focused review

- Read `docs/ROUTER.md`, the Melee spec sections 7–10 and 34, networking Melee validation and snapshot-rate sections, weapon action definitions, NPC behavior and movement documents, `docs/workflows/runtime-scenario-validation.md`, `docs/architecture/live-development/hot-audio-contract.md`, `docs/skills/spec-behavior-review-v1.md`, and the related Melee action-ID regression.
- Spec/code alignment: `PASS_WITH_HUMAN_REVIEW`. The documented NPC requirement is a 1–30 tick start-to-start interval with interruption; the previous server code required idle and added the full action duration. The existing snapshot path carries action index and start tick and the shared client presentation path triggers configured animation/audio when it sees a new action.
- No tests were added or run.

## Validation evidence

- `git diff --check`: passed. Git reported only line-ending conversion warnings for pre-existing CRLF/LF mixed files, including the changed server translation unit.
- Canonical dev-loop build request `2026-10-21T01:54:35.396Z-39907029`: success, build 2718, 1 translation unit compiled, 563 skipped; `.dev/builds/2718/mimita.exe` was published.
- `.dev/builds/2718/mimita.exe --versioninfo` succeeded and reported `EVENTS_JSONL_PATH=logs/10-20-2026/20261020_215454/events.jsonl`, run ID `20261020_215454`, PID 20660, executable path `C:\mimita-v9\.dev\builds\2718\mimita.exe`; SHA-256 `888F2F38D0EC018AD12706AE1DFA989E499DCA9AF70EB4056FC309762325E3BD`.
- The visible dev-loop session remained on build 2717, `auto_restart=false`, map `xxity2`; it was left untouched. Its pre-change journal contained NPC action starts but cannot verify build 2718's interruptions or audio. No fresh Melee scenario was launched, so do not claim the changed behavior is live or visually/hearing-verified.
- Human review still needs to confirm: a Melee NPC starts a replacement action every configured 1–30 ticks while the target is in range, the old animation is cut off, the new character-specific move animation and start/phase sounds play, and contacts from interrupted moves stop at interruption.
