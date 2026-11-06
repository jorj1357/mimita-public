# Fun Counter Strike Unrostered Startup NPC

Time created: 2026-11-05T20:06:15Z (captured run start; human report time unknown)
Time last updated: 2026-11-05T21:37:51Z

Status: ATTEMPTED FIX (FCS runtime verification pending)

Related specification:
`docs/specs/gamemodes/funcounterstrike.md` — Team identity and avatars

Related changelog:
`docs/changelog/2026-11-05/20261105_214054-fcs-team-avatar-identity.md`

---

## Regression Occurrence 1

### Observed

In captured run `logs/2026-11-05/20261105_200615`, the server journal was
previously inspected and showed a standalone NPC actor 1000 at tick 0 with
avatar `demongirl`, before FCS had assigned it a team and role. The FCS
`npc_spawn.policy` was disabled. The user's current-session report called the
unwanted follower Boomer-like; this capture establishes `demongirl`, not Boomer.

The run's raw per-process journals have since been pruned; only its `run.json`
manifest remains. Do not treat the old journal event as currently re-readable.

### Expected Behavior

FCS actors enter the match through the mode's team/role roster. Generic startup
NPCs without a mode assignment do not follow clients into the match. NPCs
intentionally created by FCS with a valid team and role remain allowed.

### Actual Behavior

The generic `--npcs` startup path created an actor before FCS roster ownership,
so the disabled persistent-spawn policy did not prevent that startup actor.

### Source Cause

`src/network/server.cpp` creates startup NPCs from `options.npcsEnabled` and
`options.npcCount`; this path is separate from the mode's persistent
`npc_spawn.policy`. `buildObjectiveRoster` also previously preserved low-ID
map/startup NPCs while rebuilding its objective roster.

### Attempted Fix 1

Time: 2026-11-05T21:40:54Z.

Change:

- Suppress generic startup NPC creation for FCS, whose authored roster owns its
  NPC population.
- Before publishing the FCS roster, remove any leftover low-ID NPC lacking an
  explicit valid mode team and role, with a bounded
  `gamemode.npc.unrostered-startup-removed` event.
- Keep intentionally assigned FCS roster NPCs and their role avatars.

Build evidence: dev-loop artifact
`2026-11-05T21:30:27.625680Z:a316f7111d218cf2317627776c1a032e2c4ee936429e4e813dd5ed8bcecc934e`,
source generation 24, compiled successfully. Its `--versioninfo` check reported
`EVENTS_JSONL_PATH=logs/11-05-2026/20261105_163335/events.jsonl`.

### Runtime Proof Still Required

Run a new FCS session and confirm no unrostered startup NPC appears while
mode-rostered CT/T NPCs retain their assigned roles and avatars. The old run's
raw JSONL has been pruned. The user-reported Boomer identity remains unresolved
until a current run records that actor ID and selected avatar.
