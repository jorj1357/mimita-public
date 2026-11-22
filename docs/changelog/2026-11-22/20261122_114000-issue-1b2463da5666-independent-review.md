# Issue-1b2463da5666 Independent Review

# Task

- Task ID: issue review sweep wake-813d078139d6
- Owner: issue-review-sweeper
- Focus: bounded independent review of issue-1b2463da5666 (Juggernaut mode NPCs do not fire at opposing actors)
- Summary: Reviewed issue readiness, proof records, linked wall/pathing issue, mode configs, the only matching Juggernaut-horde run, and the NPC roster/fire owners. Added the missing readiness context and appended one evidence review (verdict `inconclusive-needs-matching-run`, review id 169) plus one process proposal to issue-b396205aaecf. No source, build, run, close, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-22T11:40:00Z

# Pre-existing changes

- The shared checkout contains extensive pre-existing modifications from other agents. This session edited no source and reverted nothing.
- Files created by this session: this changelog. Append-only issue context, review, and suggestion were written through `agent-coord.py`.

# Requested behavior

Complete one bounded independent review of issue-1b2463da5666 without editing source, building, closing, or claiming a fix; record observed facts, inference, contrary evidence, and the next discriminating check, plus a measured process proposal when supported.

# Findings

- Readiness was score 1/8, missing reproduction, expected_behavior, acceptance, paths, evidence, proof_expectation, and risk. Added those from the existing quote, task, spec, and artifacts; readiness is now 8/8, ready=true, with all prior text preserved.
- `proof list --issue issue-1b2463da5666` returns `[]`; task `juggernaut-npc-fire-trace` has zero task_proof records.
- The only matching run, `logs/2026-11-21/20261121_011815` (jugger_horde, dust2cyberiav4, generation 417, status `stop_partial`), retains only `run.json`; no server-01 or client-01 `events.jsonl` exists. No other log or `.dev` reference to that run exists. The required same-run chain has never been captured.
- `config/gamemodes/juggernaut.json` disables NPCs on both teams (`players=false`, `npcs=false`, `npc_spawn.max_count=0`). `config/gamemodes/jugger_horde.json` enables NPCs (`fighters` players/npcs true, `juggernauts` npcs true, `npc_spawn.policy=team_roster`, `max_count=67`). `config/gamemodes/juggernaut_duel.json` also disables NPCs and targeting. The report does not name a mode.
- Owned diagnostics exist: `src/network/server-gamemode.cpp:4919-5191` roster NPC creation with role weapon sets; `src/npc/npc-combat.cpp:83-114` `npc.fire-blocked`; `:132-161` `npc.shot`; `src/npc/npc.cpp:1433` `npc.attack-gate`.

# Specification alignment

- Followed the routed issue-intake, proof-and-verification, and agent-coordination expectations: readiness and proof inspected, evidence separated from inference, no behavior claim made, prior issue text preserved.
- Respected build authority: no compiler, worker, second dev-loop, or build request was invoked.

# Exact implementation changes

- None. Append-only issue context, review, and issue suggestion only.

# Validation

- Build status: no build requested.
- Runtime evidence: read `run.json` for `20261121_011815`; confirmed no journals exist; scanned `logs` and `.dev` for other references.
- Review artifact: review id 169 on issue-1b2463da5666, verdict `inconclusive-needs-matching-run`.
- Process proposal: appended to issue-b396205aaecf (treat a run with `run.json` but zero `events*.jsonl` as explicit non-evidence; measurable target and smallest read-only experiment included).

# Measured evidence

- Matching Juggernaut-horde runs found: 1 (`20261121_011815`), with 0 `events*.jsonl`.
- Proof records for issue-1b2463da5666 and task juggernaut-npc-fire-trace: 0.
- Readiness before: 1/8; after context addition: 8/8.
- Other manifest-only examples cross-linked: 20261120_190650, 20261120_190830, 20261120_170259, 20261121_201749.

# Regression review

- Regression entry appended: no.
- Why: no behavior change; read-only review with append-only issue updates.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable; no runtime scenario was run.
- Still unverified: whether base Juggernaut or Juggernaut Horde was reported, and the full roster -> target -> attack-gate -> shot -> damage -> client chain.

# Agent handoff context

- Owner: `juggernaut-npc-stuck-shooting`; task `juggernaut-npc-fire-trace`.
- Next exact step: run one pinned `jugger_horde` scenario through the root dev-loop with at least one human Fighter, confirm both journals exist, then join per actor+tick the roster, target, attack-gate, shot, damage, and client events.
- Do not redo: this proof lookup, config comparison, and journal scan.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue context, review, and suggestion.
