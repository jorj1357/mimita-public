# Counter-Strike Checkpoint 6 — generic bomb objective and pickup/drop

Date: 2026-10-02
EST timestamp: 2026-10-02 22:51:49 EST
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, and pure objective-rule evidence are proven. Live pickup/drop
behavior and the HUD prompt are NOT visually verified and remain required.

## Scope

Checkpoint 6 of `docs/specs/20261002plan.md`: a generic objective item and
bomb pickup/drop (Stage 10). Plant/defuse/explosion and editable sites are
Checkpoint 7.

## Pre-existing / external edits (not mine)

Same runtime-written user settings as prior checkpoints. Untouched.

## Files changed

### `src/game/objective-state.h` / `.cpp` (new)

- `ObjectiveKind` (None/Bomb), `ObjectiveState`
  (Inactive/Carried/Dropped/Planted/Defused/Exploded).
- `ObjectiveInstance`: id, kind, state, position, carrierActorId,
  allowedCarrierTeam, pickupRadius, interactionRange, explosionSeconds,
  explosionDeadlineTick, active, `valid()`.
- `objectiveKindFromString`, `ObjectiveCarrierCandidate`,
  `selectObjectiveCarrier` (pure team/radius/alive/nearest selection),
  `objectiveSelfTest`.

### `src/network/server-gamemode.h` / `.cpp`

- `ServerGamemodeState` gained `objective`, `objectivePickupCounter`,
  `objectiveDropCounter`, `objectiveNextCarrierScanTick`.
- `serverCommunityStartMatch` resolves the mode's first objective definition
  and sets `allowedCarrierTeam` via `resolveTeamIndexFromId`.
- `assignObjectiveCarrier` (round start) and `serverObjectiveTick`
  (ACTIVE/GO): drop on carrier death/disconnect, follow living carrier,
  auto-pickup for the nearest eligible actor within `pickupRadius`.
- `beginObjectiveRound` calls `assignObjectiveCarrier`.
- `broadcastDuelState` replicates objective active/kind/state/team/carrier/
  position/id.
- Added `resolveTeamIndexFromId` (file-static) and structured events
  `objective.dropped` / `objective.pickup`.

### `src/network/packets.h`

`DuelStatePacket` gained objective fields (`objectiveActive`, `objectiveKind`,
`objectiveState`, `objectiveTeam`, `objectiveCarrierId`, `objectiveX/Y/Z`,
`objectiveId`).

### `src/network/community-match-client.h` / `.cpp`

Added `ReplicatedObjective` + `objective()` accessor; `onState` mirrors the
fields; `reset()` clears them.

### `src/engine/engine-tick-ui-overlays.cpp`

Added an objective prompt ("Pick up Bomb" when the camera is within 3 m of a
dropped bomb) and a "BOMB DROPPED" status for named-team modes.

### `config/gui/gamemode-meta-gui.json`

Added `objectivePrompt` and `objectiveStatus` elements to the `counterstrike`
section.

### `src/game/game-cli.cpp`

Added `--objective-selftest`.

### `docs/features/gamemodes/counterstrike.md`

Appended Attempt 6.

## Reasoning

The objective is a mode-owned runtime value, never an actor preset. Team,
radius, and alive gating live in one pure `selectObjectiveCarrier` used by
assignment, pickup, and the test, so the rules cannot drift. Drop-on-death
reuses the existing actor-state/kill pipeline. Pickup is automatic proximity for
this checkpoint; the `F` interaction path and site-gated plant/defuse come in
Stage 11/12, as the plan stages them.

## Documents and skills

- Spec: `docs/specs/20261002plan.md` (Stage 10; Checkpoint 6).
- Skill: `docs/skills/spec-behavior-review-v1.md` — no blocker findings.

## Validation

Build: `BUILD SUCCESS` (`[LINK] mimita.exe`).

Runtime (`mimita.exe`):

```text
[OBJECTIVE SELFTEST] PASS
[NPC UTILITY SELFTEST] PASS
[NPC NAV REQUEST SELFTEST] PASS
[NPC PERCEPTION SELFTEST] PASS
[GAMEMODE SELFTEST] PASS
[ACTOR PRESET SELFTEST] PASS
[CS ROUND SELFTEST] PASS
```

## Human review still needed

- In a live CS round confirm one Terrorist starts with the bomb, the bomb drops
  on that actor's death, a valid Terrorist can pick it up, a Counter-Terrorist
  cannot, and pickup does not occur through walls.
- Confirm the "Pick up Bomb" / "BOMB DROPPED" prompts appear only when valid and
  the objective state resets cleanly each round.

## Explicitly not done yet

Editable bomb sites / map JSON and site-gated plant/defuse/explosion
(Checkpoint 7); the `F` interaction path and HUD plant/defuse prompts.
