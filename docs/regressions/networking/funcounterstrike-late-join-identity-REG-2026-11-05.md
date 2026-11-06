# Fun Counter Strike Late-Join Identity Missing

Time created: 2026-11-05T20:06:15Z (captured run start; human report time unknown)
Time last updated: 2026-11-05T21:37:51Z

Status: ATTEMPTED FIX (FCS runtime verification pending)

Related specifications:
- `docs/specs/gamemodes/funcounterstrike.md` — Team identity and avatars
- `docs/specs/networking/networking.md` — Roster identity and avatar authority

Related changelog:
`docs/changelog/2026-11-05/20261105_214054-fcs-team-avatar-identity.md`

---

## Regression Occurrence 1

### Observed

The latest captured FCS run, `logs/2026-11-05/20261105_200615`, used build
`2026-11-05T20:05:47.562559Z:9a89b2f...`. Client 1's local identity was team 0
with role `fun_counter_terrorist`; client 2's local identity was team 255 with
no role. The user reported that actors on the CT client appeared enemy-colored.

The run's raw per-process journals have since been pruned; `run.json` remains
and records the run/build identity and the server's last event time. Treat the
team/role values above as evidence captured during the earlier direct journal
inspection, not as currently re-readable journal files.

### Expected Behavior

Every active FCS player is assigned a valid team and role before identity
publication. Unknown identity is not enough evidence to classify an actor as an
enemy.

### Actual Behavior

The late-join client remained unrostered and received team `255` with no role.
The currently inspected renderer suppresses team-colored outlines when team
identity is unknown, so this journal evidence establishes the missing identity
but does not independently prove the old executable's exact outline pixels.

### Confirmed Source Cause

`assignMissingRoundModePlayers` previously returned unless
`d.victoryCondition == "rounds"`. The active
`config/gamemodes/funcounterstrike.json` declares its round lifecycle under
`victory.type`/`rounds` while `victory_condition` is empty. The FCS late-join
repair therefore did not run before `broadcastDuelState` serialized the roster.

### Attempted Fix 1

Time: 2026-11-05T21:40:54Z.

Change:

- Allow the shared late-join repair to run for FCS independently of its empty
  legacy `victory_condition` field.
- Require a real roster member, valid team, and role before the recipient
  fallback can populate an identity slot.
- Select the forced role avatar on the server and include bounded identity and
  avatar transition records.
- Keep the renderer's unknown-team neutral guard.

Build evidence: dev-loop artifact
`2026-11-05T21:30:27.625680Z:a316f7111d218cf2317627776c1a032e2c4ee936429e4e813dd5ed8bcecc934e`,
source generation 24, compiled successfully. Its `--versioninfo` check reported
`EVENTS_JSONL_PATH=logs/11-05-2026/20261105_163335/events.jsonl`.

### Runtime Proof Still Required

The active shared run is Vehicle Arena and has `auto_restart=false`; it was not
replaced. Launch FCS through an isolated dev-loop session and confirm both
clients receive the authoritative team/role, then visually review the outline.
The historical captured journals are no longer present, so use new server and
client journals for this check.
