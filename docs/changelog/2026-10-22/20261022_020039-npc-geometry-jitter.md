# Open gameplay issue: player jitter and NPC geometry recovery

Time: 2026-10-22T02:00:39Z (2026-10-21 22:00:39 EDT)

Branch: `20261011zt`
HEAD: `9ec73c3ae724037da33d032d54843cbf1186f97a`

## Result

Added a user-reported issue to the October 21 intake and the issue index in
`docs/specs/open-gameplay-issues.md`. It records player jitter when far from
map geometry and NPCs remaining stuck inside geometry, with the requested
outcome that NPCs get pushed back to clear space. The entry does not claim the
symptoms share a cause or choose a collision/network recovery implementation.

## Exact change

File: `docs/specs/open-gameplay-issues.md`, intake row at line 96 and index row
at line 115.

Before the intake row, the last entry was the existing Avatar head movement
report:

```text
| Avatar head movement | Avatar heads seem to lean too far backward while moving forward. The player remembers them behaving more normally before ragdoll edits, possibly about a week earlier, but is unsure when the change began or what the intended motion was. | User-reported and uncertain; no cause or prior behavior established. | [`ragdoll-retrograd/ragdoll-retrograd.md`](ragdoll-retrograd/ragdoll-retrograd.md), [`avatar/avatar-editor.md`](avatar/avatar-editor.md) |
```

There was no prior text for the new issue. Added:

```text
| Player jitter and NPC geometry recovery | The player reports jitter when far from map geometry and NPCs remaining stuck inside geometry after recent player-world collision and networking improvements; NPCs should be pushed back out into clear space. | User-reported; not reproduced. The link between the symptoms and collision, movement correction, or networking is unconfirmed. Trace the first embedded actor position/contact and later position writers before choosing a recovery path. | [`../architecture/collision/collision.md`](../architecture/collision/collision.md), [`movement/movement.md`](movement/movement.md), [`networking/networking.md`](networking/networking.md), [`../architecture/player-npc-systems/npc-movement.md`](../architecture/player-npc-systems/npc-movement.md) |
```

The issue index previously went from the JSON settings issue directly to the
Griever issue. There was no prior text for the new index row. Added:

```text
| Player jitters far from map geometry and NPCs remain embedded in geometry | User reports both symptoms; cause and whether they share an owner are unconfirmed. NPCs should be moved back out to clear space. | [`../architecture/collision/collision.md`](../architecture/collision/collision.md), [`movement/movement.md`](movement/movement.md), [`networking/networking.md`](networking/networking.md), [`../architecture/player-npc-systems/npc-movement.md`](../architecture/player-npc-systems/npc-movement.md) |
```

## Guidance reviewed

- `docs/ROUTER.md`
- `docs/specs/open-gameplay-issues.md`
- `docs/architecture/collision/collision.md`
- `docs/specs/movement/movement.md`
- `docs/specs/networking/networking.md`
- `docs/architecture/player-npc-systems/npc-movement.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/skills/documentation-checker-v1.md`
- `docs/operations/task-completion/task-completion.md`

## Validation and remaining evidence

- `git diff --check -- docs/specs/open-gameplay-issues.md` passed; Git emitted
  only its existing LF-to-CRLF working-copy warning.
- Confirmed the changed issue rows link to current collision, movement,
  networking, and NPC movement documents.
- No code or runtime behavior changed. The reported jitter and embedded NPC
  behavior remain un-reproduced and need a live journal trace before cause or
  fix is claimed.
- Pre-existing worktree edits were present in configuration, code, and this
  issue inbox; they were preserved. No regression entry was added because this
  report is not yet confirmed as a regression.
