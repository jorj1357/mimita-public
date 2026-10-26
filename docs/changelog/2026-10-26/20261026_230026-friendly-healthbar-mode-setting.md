# Per-mode friendly healthbar setting

- **Time:** 2026-10-26T23:00:26Z (19:00:26 EDT, America/New_York).
- **Branch:** `20261011zt`.
- **HEAD at session:** `c393337d` — `reporetdly working image decal shared shader state`.
- **Result:** PASS_WITH_HUMAN_REVIEW. Added a per-mode friendly healthbar policy, enabled it for Payload, and verified the applied mode value in a live journal. Human visual acceptance remains open.

## Changes

Added nullable `presentation.friendly_healthbars` to
`config/gamemodes/masterlist/gamemode-masterlist.json` immediately after
`enemy_healthbars`:

```json
"enemy_healthbars": {"type":"boolean", "default":null, "nullable":true},
"friendly_healthbars": {"type":"boolean", "default":null, "nullable":true},
```

Added the canonical field to all 16 mode files in
`config/gamemodes/`: `bombtag.json`, `counterstrike.json`, `duel.json`,
`elimination.json`, `ffa.json`, `funcounterstrike.json`, `juggernaut.json`,
`juggernaut_duel.json`, `melee.json`, `npc_waves.json`, `payload.json`,
`retrograd.json`, `sandbox.json`, `survive_disasters.json`, `tdm.json`, and
`zombie_tower.json`. Modes inherit current behavior with `null`; Payload sets
`presentation.friendly_healthbars` to `true`.

Runtime ownership:

- `src/gamemode/gamemode.h::GamemodePresentation` owns the presence/value pair.
- `src/gamemode/gamemode.cpp` parses the masterlist-backed mode field.
- `src/network/community-match-client.cpp` applies/clears the override on mode
  changes and includes it in `client.gamemode.presentation.applied`.
- `src/gui/hud/healthbar-config.h/.cpp` owns the temporary friendly visibility
  override.
- `src/gui/hud/player-nameplates.cpp` suppresses only nonlocal friendly
  world-space bars when disabled. The local player's own bar still follows
  `config/healthbar.json`; the existing broader `hide_healthbars` policy still
  hides all world bars.

Documented the policy in `docs/specs/gamemodes/gamemodes.md` and Payload's
explicit enabled value in `docs/specs/gamemodes/payload.md`.

## Cart-size check

No issue was added for JSON cart-size editing because the setting already
exists. `config/maps/payloadsmall1.json` has `payload.cart_size` at line 30,
currently `[2.0, 1.5, 1.0]`; `src/gamemode/map-config.cpp` parses and serializes
that vector. Cart size is the full X/Y/Z body size in world units. The separate
`payload.interaction_box.size` controls the push/block area. Cart size is read
when the Payload runtime/cart entity is initialized; active-match live resizing
was not implemented or verified in this task.

## Evidence and validation

- Read the user-supplied `AGENTS.md`, `docs/ROUTER.md`,
  `docs/architecture/json-configuration/json-configuration.md`, the Payload
  and Gamemode specifications, `docs/skills/spec-behavior-review-v1.md`,
  `docs/operations/build-and-exe/build-and-exe.md`, and
  `docs/operations/task-completion/task-completion.md`.
- `python devscripts/gamemode_json_sync.py --check`: PASS.
- `git diff --check`: PASS (Git emitted only existing LF/CRLF conversion
  notices).
- Canonical dev-loop published build 3387 at
  `C:\mimita-v9\.dev\builds\3387\mimita.exe`, source current. Running
  `.dev/current/mimita.exe --versioninfo` reported build time `18:55:50` and
  `EVENTS_JSONL_PATH=logs/10-26-2026/20261026_185901/events.jsonl`.
- Live run `20261026_225958` used build 3387 in Payload mode on `payload1`.
  Its journal `logs/2026-10-26/20261026_225958/events.jsonl` contains
  `client.gamemode.presentation.applied` with
  `friendly_healthbars:true`, and `payload.cart-spawned`. This proves config
  application and cart creation on that run, not that a human visually saw the
  friendly bars. No visual acceptance was performed.

## Pre-existing worktree changes

The worktree was already dirty in multiple configs, sources, assets, evidence,
and documentation. In particular, `config/gamemodes/payload.json` already had
the player's `push_meters_per_second_per_attacker` change from `0.2` to `10.0`
and `rollback_meters_per_second` from `0.2` to `5.0`, with old-value comments.
Those values/comments were preserved; this task only added the friendly
healthbar field there.
