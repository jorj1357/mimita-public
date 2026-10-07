# Cold Build Required

Time created: 2026-09-20T15:11:13Z
Time last updated: 2026-09-20T15:11:13Z

Status: COLD-BUILD DEBT

This persistent record tracks every intentional cold build and the work needed
to move that boundary into the live-reload path. A cold build is not by itself
a user-visible behavior regression.

Related specification:
`docs/features/live-code-development/live-code-development.md`

Related workflow:
`docs/operations/build-and-exe/build-and-exe.md`

Related changelog:
`docs/changelog/2026-09-20/`

---

## Cold-build occurrence template

Copy this section for each intentional cold build. Keep all occurrences in this
file; do not create a new file for the next cold build.

### Cold-build occurrence N

Time:
`<UTC ISO 8601 timestamp>`

Related changelog:
`<exact changelog path>`

Reason the cold build was required:

<What result the session needed and why a live build was insufficient.>

Exact cold owner or boundary:

File/function/ABI/runtime mechanism:
`<exact path and owner>`

Why this is still cold:

<Evidence that the changed behavior cannot currently cross the live boundary.>

Result needed from the cold build:

<What the new executable was needed to prove or enable.>

Hot-boundary change needed:

<Smallest code or architecture change that would make this work live-reloadable.>

Migration/falsification step:

<Next test that could prove the cold dependency is removable.>

Build result:

`<SUCCESS | BLOCKED | NOT RUN>`

Human review:

<What remains unverified by source/build evidence.>

---

## Cold-build occurrence 2

Time:
`2026-09-21T04:07:29Z`

Related changelog:
`docs/changelog/2026-09-21/20260921_041000-weapon-jsonl-probes.md`

Reason the cold build was required:

The requested weapon JSONL probes were added to the cold physics translation
units so the existing process-owned structured logger could record weapon
transform and collision results. The hot DLL build cannot compile or install
those cold physics files.

Exact cold owner or boundary:

```text
src/physics/movement/physics-collision-body.cpp
src/physics/movement/physics-collision-glb-body.cpp
```

Why this is still cold:

The logger file writer is cold, and these legacy physics owners are linked into
the executable. The existing hot DLL can emit events through the logger bridge,
but it cannot replace these EXE translation units.

Result needed from the cold build:

Compile the JSONL probe calls and link a timestamped executable without
replacing or launching any existing process.

Hot-boundary change needed:

Move weapon transform/collision probing behind a generic hot collision/tool
probe capability, leaving only the logger bridge and fixed collision mechanism
in the EXE. Then the probe fields and sampling policy can be changed by hot DLL
generation alone.

Migration/falsification step:

Add a hot weapon probe producer that receives the same fixed collision result
envelope and emits the same `WEAPONS` event names. Compare both producers for
one fixed tick; once identical, remove the cold producer.

Build result:

`SUCCESS`

Human review:

The timestamped executable linked successfully, but it was not launched. The
actual runtime JSONL records and visual weapon/collision alignment remain
unverified until a game process is available.

---

## Cold-build occurrence 3

Time:
`2026-09-21T04:09:10Z`

Related changelog:
`docs/changelog/2026-09-21/20260921_041000-weapon-jsonl-probes.md`

Reason the cold build was required:

The JSONL probe coverage was extended to the legacy JSON-sphere collision path
after the first validation. A final compile was required to prove that branch
also links correctly.

Exact cold owner or boundary:

```text
src/physics/movement/physics-collision-body.cpp
```

Why this is still cold:

The changed probe remains in an EXE-linked physics translation unit. The hot
DLL was already up to date and cannot compile this cold owner.

Result needed from the cold build:

Confirm the final probe coverage compiles and links into a timestamped
executable.

Hot-boundary change needed:

Move the probe producer behind a hot collision result envelope and leave only
the stable logger bridge in the executable.

Migration/falsification step:

Compare the future hot producer against this cold producer for capsule and
JSON-sphere paths on the same simulation tick, then remove the cold probe.

Build result:

`SUCCESS`

Human review:

The timestamped executable linked successfully and was not launched. Runtime
JSONL output and visual alignment remain unverified.

---

## Cold-build occurrence 4

Time:
`2026-09-21T14:27:51Z`

Related changelog:
`docs/changelog/2026-09-21/20260921_142751-deterministic-weapon-probe.md`

Reason the cold build was required:

The deterministic weapon probe was added to the existing cold-linked
`--hot-combat-selftest`, and the weapon JSONL level/init changes touched cold
physics and self-test translation units.

Exact cold owners:

```text
src/network/hot-combat-selftest.cpp
src/physics/movement/physics-collision-body.cpp
src/physics/movement/physics-collision-glb-body.cpp
```

Build result:

`SUCCESS` — `mimita-20260921T102706.exe`

Runtime result:

The self-test ran and emitted five weapon phase records plus one transform
summary. The larger suite still exits `1` because unrelated animation phase
assertions fail, while the new weapon probe assertion passes.

Hot-boundary migration:

Move deterministic phase driving and probe emission behind the generic hot
collision/tool result envelope. Keep only a stable result envelope and logger
bridge in cold code, then compare hot and cold records on the same fixed tick
before removing the cold producer.

---

## Cold-build occurrence 5

Time:
`2026-10-07T13:02:13-04:00`

Related changelog:
`docs/changelog/2026-10-07/20261007_130200-nothing-inventory-item.md`

Reason the cold build was required:

The explicit weapon-set ownership change needed a newly linked executable to
verify the current client/server weapon and terminal owners together. The
shared `Nothing` definition and equip behavior are cold-linked gameplay code;
the hot DLL was already up to date.

Exact cold owners:

```text
config/weaponsets.json
src/combat/weapon-system.cpp
src/combat/weapon-system-equip.cpp
src/network/server-packets.cpp
```

Build result:

`SUCCESS` — `mimita-20261007T-nothing-item.exe`

Runtime result:

`--versioninfo` passed and wrote
`logs/10-07-2026/20261007_130130/events.jsonl`. A connected gameplay run was
not performed, so human empty-hand switching remains unverified.

Hot-boundary change needed:

Move weapon-set ownership resolution and the empty-hand equip envelope behind
the hot weapon/action bridge while retaining server authority and the stable
network snapshot contract.

---

## Cold-build occurrence 6

Time:
`2026-10-07T13:12:59-04:00`

Related changelog:
`docs/changelog/2026-10-07/20261007_130200-nothing-inventory-item.md`

Reason the cold build was required:

The number-key toggle and hotbar/crosshair presentation owners are cold-linked
client translation units. A new executable was required to verify the HUD and
input changes together.

Exact cold owners:

```text
src/engine/engine-tick-combat.cpp
src/engine/engine-tick-ui-game-hud.cpp
src/combat/weapon-system.cpp
```

Build result:

`SUCCESS` — `mimita-20261007T-hidden-nothing-toggle-v2.exe`

Runtime result:

`--versioninfo` passed and wrote
`logs/10-07-2026/20261007_131257/events.jsonl`. Connected key-toggle and
visual presentation acceptance remain unverified.

Hot-boundary change needed:

Move input toggle intent and the Nothing visibility policy behind the hot
action/presentation bridge while keeping server equip authority unchanged.

---

## Cold-build occurrence 7

Time:
`2026-10-07T13:13:26-04:00`

Related changelog:
`docs/changelog/2026-10-07/20261007_130200-nothing-inventory-item.md`

Reason the cold build was required:

The final adjustment changed the cold-linked Juggernaut picker/input owner so
repeat-key toggling also works after the fighter choice is committed.

Exact cold owner:

```text
src/engine/engine-tick-combat.cpp
```

Build result:

`SUCCESS` — `mimita-20261007T-hidden-nothing-toggle-v3.exe`

Runtime result:

`--versioninfo` passed and wrote
`logs/10-07-2026/20261007_131326/events.jsonl`. Connected gameplay acceptance
remains unverified.

Hot-boundary change needed:

Move the picker visibility/toggle decision behind the hot input-action bridge
while retaining the same shared terminal action.

---

## Cold-build occurrence 8

Time:
`2026-10-07T13:51:27-04:00`

Related changelog:
`docs/changelog/2026-10-07/20261007_130200-nothing-inventory-item.md`

Reason the cold build was required:

The repeat-key decision was moved into the cold-linked terminal weapon-command
owner so custom key bindings and number keys share one action path.

Exact cold owner:

```text
src/terminal/weapon-commands.cpp
```

Build result:

`SUCCESS` — `mimita-20261007T-repeat-key-unequip.exe`

Runtime result:

`--versioninfo` passed and wrote
`logs/10-07-2026/20261007_135123/events.jsonl`. Live key-binding gameplay
acceptance remains unverified.

Hot-boundary change needed:

Move the shared equip-toggle action behind the hot terminal/action bridge
while retaining server-authoritative equip replication.

---

## Cold-build occurrence 9

Time:
`2026-10-07T14:00:00-04:00`

Related changelog:
`docs/changelog/2026-10-07/20261007_130200-nothing-inventory-item.md`

Reason the cold build was required:

Friendly-fire ownership changed in the cold-linked gamemode parser and
authoritative server damage/projectile owners.

Exact cold owners:

```text
src/gamemode/gamemode.cpp
src/network/server-gamemode.cpp
src/network/server-damage.cpp
src/network/server-packet-handlers.cpp
src/network/server-projectiles.cpp
```

Build result:

`SUCCESS` — `mimita-20261007T-friendly-fire-policy.exe`

Runtime result:

`--versioninfo` passed and wrote
`logs/10-07-2026/20261007_193923/events.jsonl`. Live mode acceptance remains
unverified.

---

## Cold-build occurrence 10

Time:
`2026-10-07T15:01:00-04:00`

Related changelog:
`docs/changelog/2026-10-07/20261007_130200-nothing-inventory-item.md`

Reason the cold build was required:

Reset the friendly-fire policy when the authoritative server mode state starts,
so a prior mode cannot leak its setting into the next match.

Exact cold owner:

```text
src/network/server-gamemode.cpp
```

Build result:

`SUCCESS` — `mimita-20261007T-friendly-fire-policy-v2.exe`

Runtime result:

`--versioninfo` passed and wrote
`logs/10-07-2026/20261007_193958/events.jsonl`. Live mode acceptance remains
unverified.
