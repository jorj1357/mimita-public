# Gameplay issue intake and autonomy review

## Task

- Agent: `juggernaut-npc-stuck-shooting`
- Branch: `checkpoint/one-checkout-condensation-20261111`
- HEAD: `ce0009d5b2983f462d1d17dd640c4e66769c2a23` (no commit created)
- Date/time: 2026-11-20 06:55 UTC
- Result: issue intake and static investigation complete; behavior proof pending.

## User reports and issue routing

- Created `issue-1b2463da5666` for Juggernaut NPCs not firing; appended the
  follow-up to existing NPC wall/pathing issue `issue-82cdc1277704`.
- Created `issue-55bd775c8026` for the dev-loop GUI new-server connect failure.
- Resumed existing Arcade capsule issue `issue-da337d1fe2f3`, preserving the
  user's reports about capsule penetration, animated-pose snapping, cylinder
  velocity loss, and disabled weapon/world collision in Arcade.
- Reused `issue-503f3e410c01` for the Arcade weapon-collision report and
  `issue-04079f42c6f9` for visible melee contact failures; no duplicate melee
  issue was created.
- Updated autonomy/context issue `issue-91e3384b219b` with the concrete
  investigation gaps and capability suggestions.

## Findings

### NPC configuration and available evidence

- `config/gamemodes/juggernaut.json` disables players and NPCs on all teams.
  `jugger_horde.json` enables NPCs on both teams; `juggernaut_duel.json` uses
  `npc_spawn.policy=team_roster` and its spec describes a solo opposing NPC.
  Therefore the exact runtime `gamemode` ID and roster policy determine whether
  NPCs should exist.
- NPC owner diagnostics are present: `npc.target-changed` in
  `src/network/server-npcs.cpp`, and `npc.fire-blocked`, `npc.shot`, and
  `npc.fire-gate.module` in `src/npc/npc-combat.cpp`.
- Peer review found the recent TDM run `20261120_064201` has NPC stuck/recovery
  episodes but is not a Juggernaut reproduction. The missing proof is a
  same-run roster/team → target → fire gate → shot → accepted damage → client
  receipt trace.

### Dev-loop GUI connection report

- Run `20261120_063206`, request `dashboard-d14f90e4`, started a server but no
  client and timed out waiting for server gameplay readiness. Its arguments
  contained duplicate `--mode` values. This is not the reported client crash.
- Closest client case is run `20261120_063800_01`, request
  `dashboard-b2da2248`, Payload/funworld3, artifact build ID
  `2026-11-20T05:38:11.772319Z:06e0eafc8e0291960e9d19162dd5e6355d49d88d702b6febf7a6e6890d5a545b`.
  The launch report recorded `client_connect_timeout` after 45 seconds for PID
  22852. That client requested and preflighted connection to
  `127.0.0.1:55589` on funworld3 at 06:38:43. At 06:40:47, the same PID
  requested connection to `127.0.0.1:1357` on badhouse; it applied a snapshot
  and spawn at 06:40:52 and continued receiving snapshots. Run metadata later
  marked that client stale. This proves a readiness/connection timeout and a
  delayed/replaced connection, not a process crash.
- Request `dashboard-c4d45252` at 06:51 failed client reconciliation because
  PID 22852 was still alive. No crash artifact or exact user old-room → new
  mode sequence is identified.
- Autonomy gap: `launch.json` is not a complete evidence bundle; run IDs,
  resolved mode/map, PIDs, journal paths and process/crash evidence require
  correlation with `stages.jsonl`, dev-loop events, `run.json`, and child
  journals. `docs/specs/dev-loop/launch-feedback.md` already defines most of
  the intended fields. Record this linkage on new launch reports and preserve
  the GUI request ID.

### Arcade capsule, weapon collision and melee

- In `src/physics/movement/physics-collision-glb-main.cpp`, Arcade skips the
  actor-triangle and body/weapon movement collision phases and takes the root
  capsule path.
- `containActorFinalPose` in `src/physics/movement/actor-final-pose.cpp` also
  samples animated actor meshes and weapon spheres and can translate the root.
  It is called without an Arcade guard after player weapon updates in
  `src/engine/engine-tick-combat.cpp` and after NPC animation in
  `src/npc/npc.cpp`. This is a plausible way limb/weapon pose can still move an
  actor root in Arcade; confirm with pinned runtime correction/contact events
  before changing behavior.
- A peer reviewer found existing melee issue `issue-04079f42c6f9` records a
  pinned run with 171 client claims/123 batches and zero server
  `melee.contact.batch`/claim decisions. The first evidence gap was server
  dispatch/receipt; this needs a current pinned reproduction.
- Another active investigator reported an Arcade early return in the server
  physical-contact path may also skip damaging melee contacts. This must be
  traced separately: turning off weapon-vs-world physical collision should
  not remove authoritative melee damage contacts.
- There is no runtime evidence yet for the user's capsule sticking/velocity
  loss on cylinders. The older physical-object bounce regression is not proof
  of player capsule behavior.

## Autonomy and collaboration review

- Three peer reviews were requested and received for NPCs, Arcade/melee, and
  GUI connect/autonomy. Their relevant findings are recorded on the existing
  issue discussions.
- AI can inspect source, configuration, dev-loop request stages, run manifests,
  and process journals, and can ask peers for independent review. In this case
  it correlated an adjacent connect timeout without mislabeling it a crash.
- Full resolution remains blocked by lack of a matching user crash/run and by
  the shared live runtime being owned by another task. Do not replace or stop
  that run. A safe isolated pinned scenario and launch reports that directly
  link request → run → mode/map → PIDs/journals/crash artifacts are the smallest
  useful autonomy improvements.

## Validation and proof status

- Source/config/journal inspection only; no compiler, build, or tests were run.
- No source changes were made and no runtime scenario was started or stopped.
- Shared dev-loop status at review time: active run `20261120_063800_01`,
  Payload/funworld3, game generation 397; published EXE build ID above had
  source generation 0 and was stale relative to the checkout.
- Build evidence, runtime proof, visual/human acceptance, and deployment are
  separate; none are claimed for these reports.
