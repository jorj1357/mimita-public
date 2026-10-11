2026 10 11 0337 jorj to do exlaplin 

u add logging, run the exe after logs are included, check logs in the events.jsonl, make deciisons  based on those values  like implement fix if its clear, more logging if unclear

2026 10 11 1138 jorj todo  this too

test and ensure that  the damage voluem works, like , starting a real .exe and teleporting to the damage volume and looking at events.jsonl to prove in a live game  it does damage to me with events.jsonl

like mkae this doc more general pretty much  bc it seems kinda just for weapons work 

# Live AI Fix Procedure

Date: 2026-10-11

## Purpose

This document defines the evidence-first procedure for fixing a gameplay or
runtime problem while the MiMITA development loop is active. It is for an AI
agent or developer who needs to determine the first broken owner, make the
smallest safe change, and prove the change in the real runtime.

This is a workflow specification, not a replacement for `AGENTS.md`,
`docs/ROUTER.md`, the feature specification, or the runtime/build procedures.

## Does not

- Treat a successful compile as proof that the feature works.
- Guess that the executable is stale before tracing the input-to-output path.
- Add a synthetic self-test before observing the real scenario.
- Use a new hotbar slot as a workaround for a weapon-identity or permission
  problem.
- Claim multiplayer, visual, audio, or human acceptance without observing it.

## Required sequence

### 1. Load the route and define the observable failure

Read `AGENTS.md`, `docs/ROUTER.md`, the selected specification and architecture
documents, the relevant workflow/skill documents, and the current changelog
state. Write the failure as an observable statement:

> actor, command/input, expected state, actual state, runtime identity, and
> exact evidence location.

For example: “In sandbox, `equipweapon heavy_shotgun` should equip the Heavy
Shotgun without changing the 1–9 hotbar; the actual weapon remains unchanged;
the run is identified by `EVENTS_JSONL_PATH=...`.”

### 2. Find the authoritative live run

Use `python devscripts/dev-loop.py --status` and the dev-loop control path to
identify the current build, client/server processes, profile, and journal. If
the journal is not known, find it from the exact executable's
`--versioninfo`; never infer it from a similarly named old run. Do not launch a
second loop when one already owns the repository/runtime.

### 3. Trace the complete chain

Follow the real path in order, naming the owner and evidence at every boundary:

`terminal/input -> command handler -> local state change -> logical/native
slot translation -> outbound packet -> server role/mode/set permission ->
server accepted/rejected state -> replicated/current weapon -> presentation`

For weapon equip, keep these values distinct:

- weapon identity, such as `heavy_shotgun`;
- native definition slot, used by the local `WeaponSystem`;
- logical community slot, used by `equipslot(n)` and network validation;
- active weapon set/role, which is the authority for permission.

If the desired action is identity-based, prefer `equipweapon <weapon_id>` over
renumbering the stable 1–9 hotbar. The command must still resolve through the
active set and must not bypass server authority.

### 4. Add bounded owner-level evidence

If the existing journal cannot identify the first divergence, add or improve a
`StructuredLogger` event at the owner where the decision is made. Include only
fields needed to join the event to the run and compare expected with actual:

- run/build/generation or executable identity;
- tick, local player/client identity, role/mode, and active set;
- command/input and requested identity or slot;
- logical slot, native slot, old state, new state;
- accepted/rejected result and a reason.

Keep the event categorized, rate-limited where appropriate, and hot-reloadable
under the existing logging configuration. Do not create unmanaged debug files.

### 5. Build and exercise the real scenario

Use the repository-root dev loop as the canonical live build/run path:

```text
python devscripts/dev-loop.py
```

Use `--status` or the documented control options when a loop is already
running. Do not manually launch a stale executable or use `build_agent.py` as
a substitute for live runtime proof. After a code change, capture the exact
new numbered build executable with `mimita.exe --versioninfo`, record its
`EVENTS_JSONL_PATH`, then perform the real command/input sequence in the real
game mode.

For the weapon-equip case, the acceptance sequence is:

1. start or join the intended sandbox/role;
2. issue `equipweapon heavy_shotgun`;
3. verify the command reports the weapon identity and both slot values;
4. verify `weapon.equip.command_accepted`, server equip acceptance, and the
   current weapon agree in `events.jsonl`;
5. verify the existing 1–9 hotbar mapping did not change.

### 6. Inspect the live journal and decide

Read the new run's `events.jsonl` after the scenario. Compare expected and
actual values and identify the first divergence, not merely the final symptom.

- If the evidence names a specific owner and the proposed change closes that
  boundary, implement the smallest patch.
- If the evidence is incomplete or contradictory, improve the owner-level
  logging and repeat the real scenario.
- If the server rejects the action by role/set policy, fix the role/set
  configuration or report the intentional restriction; do not bypass it in the
  client.

### 7. Report separate evidence

The completion report must separate:

- source/configuration change;
- build/dev-loop identity and result;
- runtime journal evidence and first-divergence result;
- human-visible or multiplayer acceptance, if directly observed;
- remaining limitations or unverified claims.

Every repository-touching session ends with exactly one final changelog entry.
