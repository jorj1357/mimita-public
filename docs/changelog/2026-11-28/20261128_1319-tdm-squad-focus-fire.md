# TDM squad focus-fire, fair reacquisition, FCS CT site split, moving support

# Task

- Agent: `tdm-npc-focus-fire`
- Issue: `issue-bbd7b6567300`
- Requested behavior: TDM NPCs must keep fighting at maximum intensity for the
  whole match, both teams must act as one unit and attack the same target, and
  the human must remain in the target set without any human-specific priority.
  FCS Terrorists commit to one site; CTs split across sites and converge on the
  first sighting. A human must collide with the Payload cart and stand on it
  without gaining speed.
- Date: 2026-11-28T13:19:49-05:00 (18:19 UTC)

# Specification alignment

- `docs/specs/npc-behavior/npc-behavior.md` section 15 (new): elite squad focus
  is the shared travel **and aim** target; no human priority; fair stale-target
  rotation; generic damage retaliation.
- `docs/specs/gamemodes/teamdeathmatch.md` (new section): continuous
  focus-fire, one shared target per team, no human priority, reacquisition.
- `docs/specs/gamemodes/funcounterstrike.md`: CT defenders actually split
  round-robin over usable sites; one team report pulls every CT to the sighted
  enemy area; CTs infer the site and never receive the T site choice.

# Exact implementation changes

| File | Change |
|---|---|
| `src/npc/team-brain.h` | Adds per-enemy `lastTargetedTick`, per-defender `assignedSitePositions`, `focusHoldUntilTick`, and `SquadTuning::focusReacquireSeconds`. Squad focus comment now says shared travel **and aim** target. |
| `src/npc/team-brain.cpp` | CT defenders get a real per-actor site assignment (round-robin over usable sites); `objectiveTargetPosition` uses the assigned site only when no team report exists, so a sighting still converges the squad. |
| `src/network/server-gamemode.cpp` | Focus is validated against the living hostile set; TDM (no objective commitment) uses nearest living enemy; enemies ignored for 8 s within 150 m become stale-priority; focus hold uses the elite `focus_reacquire_seconds`; `npc.focus-changed` now carries `focus_actor` and `mode`. |
| `src/network/server-npcs.cpp` | All elite-squad NPCs use the team's `squad().focusActorId` as the shared aim target (`target_reason=squad_focus`); invalid focus falls back to normal selection. Updates `lastTargetedTick` every active tick. Generic NPC-damage retaliation marks the attacker stale-priority for the victim's team. |
| `src/network/server-damage.cpp` | Player/NPC damage marks the attacker as the victim team's next stale-priority focus candidate. Identical for humans and NPCs. |
| `src/npc/npc-behavior.cpp` | Elite `maxAttackersPerTarget` 3 → 16 so the whole living squad gets attack slots. |
| `src/physics/movement/physics-collision-glb-sweep-slide.cpp` | Removed support-velocity inheritance when leaving moving support. A kinematic platform carries the actor by position but no longer launches it or grants speed. |
| `src/network/server-packet-handlers.cpp` | Comment-only cache repair to force the skipped TU to rebuild (see build friction). |

No behavior JSON was edited. No human-priority flag is enabled; the default is
one NPC = one actor = one human for targeting.

# Diagnostics

- `npc.focus-changed` now includes `focus_actor` and `mode`, so shared-target
  proof can name the focused actor.
- Existing `npc.target-changed` carries `target_reason=squad_focus`.
- Existing `server.damage.applied` and `npc.stuck-episode` remain the combat and
  movement evidence.

# Validation

## Build

- Build approvals: `build-request-79a51f43428b` (first, failed codegen),
  `build-request-47e40bf993fa` (blocked), `build-request-eb6dfad30815`
  (compile-order fix), `build-request-9da5f406d3a4` (link retry),
  `build-request-e295f096a55b` (cold cache rebuild, timed out at 1080 s),
  `build-request-12597f8bba32` (warm incremental retry → generation 825),
  `build-request-cb8ecde49504` (focus hold → generation 826).
- Final artifact: generation 826, `build/game/generation-826/game-g826.dll`,
  revision `0bb9684fecab7ee5cf946c634d0f3659b7c064fd093f957b7850bcde18fa7b22`,
  code hash `6c4bdcfc564d9e8b03d81e3f27d2f3caa37bc8c81f0f6f6b1d4df7463a594ec4`.
- Final compile: 30 TUs, 107.985 s compile, 4.25 s link, 123.5 s total,
  617 skipped.
- Build-friction repairs:
  - `config/hot-reload/packet-ledger.json` was stale from another agent's
    packet edits. The canonical `devscripts/packet_registry_codegen.py` was run;
    `--check` passed. Generated files were not hand-edited.
  - First compile failure: `thinkingDue` used before declaration in
    `server-npcs.cpp`; the focus block was moved after the thinking-cadence
    computation.
  - Two distinct undefined-symbol link failures on cached TUs
    (`handleSpyKnifeHitClaim`, `tickServerAnimatedPhysicalContactWeapons`).
    Per the build self-repair runbook the object cache was quarantined:
    `build/game/objects/b10e3a31b3ae8637` →
    `build/game/objects/b10e3a31b3ae8637.bak-20261128T174023Z`.
  - The cold rebuild built 1,212 objects before the 18-minute attempt cap. The
    warm incremental retry then published generation 825. Generation 826 adds
    the focus hold.

## Runtime proof

- Pinned run: `20261128_181614`, `tdm-validation`, map `dust2cyberiav4`,
  generation 826. Journals:
  `logs/2026-11-28/20261128_181614/server-01/events.jsonl` and
  `.../client-01/events.jsonl`.
- Proof record: `task-proof-a52f080c35e5` (verified, proven).
- Persisted measurement: `.dev/evidence/20261128-tdm-focus-fire.json`.
- Results (server journal, ~130 s active window):
  - 274 `npc.target-changed` events with `target_reason=squad_focus`; 20
    `module_scored`.
  - 42/42 one-second samples with at least two sampled team members had **100%**
    of sampled members on the same `server_target_id`.
  - Enemy team 1 targeted the human `actor 1` in 106 target changes, the
    most-targeted actor on that team. Human and NPC enemies are in the same pool.
  - 754 `npc.focus-changed` events carry `focus_actor`/`mode`.
  - 76 NPC-vs-NPC `server.damage.applied` events in the run.
  - Focus hold reduced focus changes from 4,248 target changes in the pre-hold
    130 s sample (run `20261128_180550`, generation 825) to 630 focus changes in
    the post-hold 130 s sample (run `20261128_181614`, generation 826).
- Limitation: the isolated `tdm-validation` client stays at long range; this run
  proves acquisition and shared focus, not NPC-to-human damage. The earlier live
  journal (`logs/2026-11-28/20261128_092602/client-01/events-0010.jsonl`) showed
  the human being targeted and killed at close range before the fix.

# Human acceptance

Not performed. Visible gameplay feel (five NPCs turning a corner and committing
to one target, FCS CT split/convergence, and standing on the moving cart) still
needs human playtest review.

# How to reverse

- `git checkout -- <file>` for the source files listed above.
- Restore the old telemetry DLL generation if needed; do not assume the object
  cache is reversible.
- The quarantined object cache is retained at
  `build/game/objects/b10e3a31b3ae8637.bak-20261128T174023Z`; do not delete it
  without review.

# Related issues

- `issue-bbd7b6567300` (TDM target loss; this fix)
- `issue-b18e63894585` (NPC corner/passivity; focus-fire and rotation help, nav
  remains a separate follow-up)
- `issue-87d8c9c7ffb1` (Payload cart collision; moving-support velocity change)
- `issue-1d1b9c62ac08` (Payload/mimita-pipes NPC ground failure; still the first
  blocker for NPC nav/attack/defend there)
