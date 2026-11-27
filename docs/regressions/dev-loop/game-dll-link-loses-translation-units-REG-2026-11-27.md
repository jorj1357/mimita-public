# Regression: the game-DLL link loses whole translation units and the build cannot converge

Recorded 2026-11-27 by `rocket-league-input2` (session agent `rocket-league-feel`
started the investigation). Append-only record; do not edit earlier occurrences.

## Symptom

Every game-DLL request fails at link with `undefined symbol` errors, and each
attempt fails on a **different** symbol. No generation is published, so no source
change reaches the runtime.

Sequence observed in one hour, same flags identity `b10e3a31b3ae8637`:

| Attempt | Undefined symbol(s) | Home TU |
|---|---|---|
| `game-dll-20261127T005142888202Z-0200fc` | `PlayerAiController::{buildFrame,setActive,meleeWeapon,setMeleeWeapon}`, `ClientCollisionWorldView` ctor + vtable | `src/actor/player-ai.cpp`, `src/combat/client-collision-world-view.cpp` |
| `...T012317214621Z-4a732c` | `effectiveGameplayMouseButton`, `advanceScheduledTerminalMoveFixedTick` | `src/input/input-poll.cpp` |
| `...T034952585428Z-210073` | `advanceScheduledTerminalMoveFixedTick` (link only, 1.6 s) | `src/input/input-poll.cpp` |
| next | `MimitaNet::CommunityMatchClient::fighterWeaponName` | `src/network/community-match-client.cpp` |
| next | symbols referenced by `map-editor-host-bridge.cpp` | `src/mapeditor`/`src/game` |

In every case the definitions **exist in the working tree**. They are missing
from the link, not from the source.

## Evidence

- `build/game/progress.log`: `sources=644 jobs=10`, then `link failed in 1.625s`
  with `collect2.exe: error: ld returned 1 exit status`.
- Build summary `worker_metrics`: `compiled: 1`, `skipped: 644`,
  `link_input_count: 644` — the link runs without compiling anything.
- Touching a file's mtime does **not** invalidate its object (verified twice).
  Changing its **content** recompiles that one TU.
- `.dev/build-timings/game-dll-20261127T005644836418Z-43dc27.jsonl` records
  `src/vehicles/vehicle-system.cpp` with `status: skipped` although the file had
  been edited, and the published generation did not contain the change.
- `build/game/objects/b10e3a31b3ae8637` held 4594 files.

## Root cause (inferred, flagged as such)

The link input list is not reconciled against the current source list before
linking. When an object file for a live source is absent (cleaned, moved,
replaced by a rebuild under a different hash, or never produced because its
rebuild was skipped), the link simply omits that TU. Because only changed TUs are
compiled, the missing entry is never recreated — the failure just moves to the
next missing TU on the following attempt. Content edits repair exactly one TU,
which is why the loop cannot converge.

This is the same family as the earlier "changed source reported as skipped"
occurrence in this document, escalated: a *skipped* source and a *missing link
input* are the same defect seen from two sides.

## Overrides applied (human-authorized 2026-11-27 ~04:00Z)

The user explicitly authorized a clean rebuild and any override needed, and asked
that every override be recorded. What was done:

1. **Quarantined the object cache** for flags identity `b10e3a31b3ae8637`:
   `build/game/objects/b10e3a31b3ae8637` (4594 files) was **moved**, not deleted,
   to `build/game/objects/b10e3a31b3ae8637.bak-20261127-0400`, then a normal
   `dev-loop.py --request game-dll` forced a full recompile. Moving rather than
   deleting preserves the ability to compare a stale object later.
2. **Two comment-only content edits** were made purely to force a relink of TUs
   whose mtime had been ignored: `src/input/input-poll.cpp` and
   `src/network/community-match-client.cpp`. No behaviour changed in either.
3. **No compiler, build script, or second dev-loop was invoked.** Every build
   went through `dev-loop.py --request game-dll`, per gate 0.

## Prevention (so it cannot happen again)

1. Derive the link input list from the **source list**, not from a manifest
   carried between attempts.
2. Before linking, assert that every source in the list has a current object and
   fail with the **list of missing TUs** (count and names), not the first
   undefined symbol.
3. Invalidate the object on **content hash**, never on mtime alone, and record
   that hash in the attempt summary.
4. Record the object-cache identity (`flags_identity`) and its file count in
   every attempt summary so a shrink is visible.
5. Treat "link failed but `compiled == 0`" as a cache fault, not a code fault, and
   say so in the failure message.

## Self-repair (what an agent should be able to do without a human)

Proposed, not yet implemented:

1. `dev-loop.py --request game-dll --self-repair` (or automatic on a
   `compiled == 0` link failure): enumerate sources, enumerate objects, and
   recompile only the missing set.
2. If the missing set is larger than a threshold (say 8) or the same TU is
   missing twice in a row, quarantine the object cache for that flags identity to
   `objects/<id>.bak-<utc>` and do one full rebuild, then resume.
3. Emit `build.self-repair` with `reason`, `missing_count`, `quarantined_dir`,
   `recompiled`, `generation` so the action is visible in `events.jsonl` and in
   the attempt summary.
4. Never delete objects: quarantine so a later comparison is possible.
5. Serialise self-repair under the existing single-builder rule so two agents
   cannot quarantine the same directory concurrently.

Until that exists, the manual runbook that worked is: move
`build/game/objects/<flags_identity>` aside, `--request game-dll`, then verify the
generation contains the intended change by checking `changed_sources` in
`build/game/current.json` and a runtime event unique to that change.

## Status

Open. The quarantine + full rebuild was started 2026-11-27 ~04:00Z; publication
and the input fix verification were still pending when this record was written.
