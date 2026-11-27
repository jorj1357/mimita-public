# Independent bounded review of issue-503f3e410c01

- Task ID: wake-7f2f07acdcd8
- Agent: issue-review-sweeper
- Focus: one read-only review of the melee/arcade/Mace-active-pose issue, its readiness and proof state, the current-build 2026-11-27 journals, and the 2026-11-27 client-spawned-server evidence
- Summary: Appended review id 503 (verdict `inconclusive`) to `issue-503f3e410c01`, and one evidence-backed process proposal to `issue-b396205aaecf`. No source edit, build request, test run, close, owner change, or fix claim.
- Status: complete (review only)
- Date, time, timezone: 2026-11-27T01:13Z, ISO 8601

# Pre-existing changes

- The shared checkout contains extensive pre-existing modifications from other agents and humans (config files, `devscripts/*`, `src/*`, `docs/*`, many changelogs and issue docs). This session edited no source, reverted nothing, and touched only the issue records through `agent-coord.py` and this changelog. No compiler, `build.py`, module worker, second dev-loop, or runtime test run was invoked.

# Requested behavior

Run one bounded independent review of `issue-503f3e410c01`: inspect issue brief/readiness, proof status, linked artifacts, and the latest relevant journals; append one evidence-grounded review with observed facts, inference/uncertainty, contrary evidence, and a next discriminating check; then a short development-process review with one specific proposal to the best existing improvement issue (or none if unsupported). Do not edit source, build, close, or claim a fix.

# Specification alignment

- AGENTS.md gates 0b/0c/0d, 2, 3, 4: coordination begin/sync/observe; observations separated from inference; no compiler/build/second dev-loop in a review wake; preserve prior append-only text.
- `docs/specs/dev-loop/agent-proof-and-verification.md`: a build or live process is not behavior proof; acceptance needs a pinned run journal with the required event.
- `docs/specs/weapons/melee/melee-weapons.md` (issue path): active-pose multiplier, Arcade contact gating, and white viewmodel acceptance.

# Exact implementation changes

None. No source, specification, or configuration change.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md` | 5 reviews (ids 52/206/300/395/446) | + review id 503 | required independent review record | remove only the appended review text |
| `docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md` | 12 suggestions (through 2026-11-27T00:10Z) | + one process proposal | required process-improvement record | remove only the appended proposal text |
| `.dev/agent-coord/brain.db` | no review id 503 | review id 503 | coordination record | restore from backup if withdrawn |
| `docs/changelog/2026-11-27/20261127_0113-issue-503f3e410c01-independent-review.md` | absent | new changelog | one changelog per repository-touching session | delete file |

# Contract / ABI changes

None.

# State / schema changes

- `issue_review` added review id 503 (`issue-review-sweeper`, 2026-11-27T01:12:59.370Z) to `issue-503f3e410c01`; state stays `in_review`, owner unchanged (`melee-arcade-fix`), 3 tasks remain open/unproven.
- `issue_suggest` appended one process proposal to `issue-b396205aaecf` (`updated_utc` ~2026-11-27T01:14Z).

# Diagnostics

- Readiness: `ready=true`, score 8/8, missing=[], state `in_review`, 3 open tasks (`arcade_no_weapon_world_collision`, `arcade_npc_melee_hit`, `mace_active_contact`).
- Proof: `proof status` still reports all three tasks as status=`runtime_proven` with result=`unproven`, reason="", run_id=`20261120_072010`, game_generation=398, journal_path="", verified_utc=""; `proof list --issue issue-503f3e410c01` returns `[]`.
- Source-current build: run artifact `2026-11-26T22:52:33.481041Z:cb72ee40...d781c` (source revision 7176d28a, dirty worktree c2ccc4c7) loaded game DLL `build/game/generation-742/game-g742.dll` sha256 `02bd9fa326519379e1a84c7307e7e42c0b0294b4d3eaef9c79b126b3f4b6be40` (built 2026-11-26T23:23Z). weapon-viewmodel.cpp on disk is `3d1ef8c1...` = commit 7176d28a and is clean.
- Run `20261127_000428`, `client-01/events-0001.jsonl`: 237 client claims, 88 accepted, 78 exact; 10 active Mace accepted (mult 10.0, damage 250..2351), all 10 both-active exact; 0 future_contact; arcade_mode=true 644/644; pose_skip 741 (383 nonzero parts); 512 NPC->player physical_contact; NPC->player weapon.contact.result chainsaw 51 / mace 8 / swordsword 6.
- Run `20261127_001718`, `client-01/events.jsonl`: 30 claims, 13 accepted, 12/13 exact; 1 active Mace accepted (damage 2304, mult 10.0) + 1 inactive Mace; 0 future_contact; arcade_mode=true 699/699; pose_skip 812 (421 nonzero); 107 NPC->player physical_contact; NPC->player weapon.contact.result mace 1 / chainsaw 5 / swordsword 7.
- Identity: accepted melee events are emitted by client-spawned GUI servers (`network.gui-server.launch`): pid 20596 (parent 24528) in run 000428; pids 4660 then 30816 (parent 29544) in run 001718, all on dust2cyberiav4/team_deathmatch. Both clients joined the dedicated server-01 (127.0.0.1:60364, atdm) then disconnected with reason `local_shutdown` within ~3s. The pinned server-01 (pid 11572) journal has 0 NPC->player damage and 0/1094 nonzero-part pose skips; `run.json` names only server-01.
- White active pose: source sets white tint (src/combat/weapon-viewmodel.cpp:270-276, :693-698) but the only emission is a `DebugConfig::DEBUG_WEAPON_VIEWMODEL` printf (:703-707); zero tint/viewmodel journal events.

# Validation

- `agent-coord.py begin/sync/observe`, `issue show/readiness`, `proof list`, `proof status`, journal parsing with Python, `git log`/`Get-FileHash`, CIM parent-process inspection, `dev-loop.py --status` (read-only).
- `issue review` appended id 503; `issue suggest` appended one proposal to `issue-b396205aaecf`.
- No compiler, build request, module worker, second dev-loop, or runtime test run was invoked.

# Findings

1. This is the strongest source-current player-Mace evidence to date: on a build made after the current weapon-viewmodel commit, active-pose Mace produced server-accepted 10x damage (250..2351) with exact client/server prediction on all both-active contacts, zero future_contact rejects, and Arcade active in every sampled capsule solve.
2. The evidence is still not attachable as acceptance proof because the accepting server is a client-spawned GUI server on dust2cyberiav4, not the run's pinned dedicated server-01 on atdm, and `run.json` does not name the joined/validating server.
3. The white Mace active-pose visual remains source-only; there is no machine-readable tint event and no captured visual.
4. The three proof rows remain unverified and stale-by-hash (weapon-viewmodel 1/4, server-players 2/2, physics-collision 2/2), and `related_build`/`related_run` still cite the failed gen398 runs.
5. NPC->player melee under Arcade has fresh support on the same build but, again, only in the client-spawned server.

# Confidence

High on the run counts, parity, Arcade flags, pose-skip nonzero parts, and process-parent identity (direct journal reads and CIM). Moderate on "source-current": the builds were dirty at build start and the game DLL generation 742 revision is not directly recorded in `run.json`. Unknown: human vs synthetic input (no agent-command entries; `source=input` only).

# Proof

No proof claim. Read-only review; evidence is journal-tier and runtime-state-tier. No behavior change introduced.

# Regression review

- Regression entry appended: no
- Why: no confirmed code change or fix; observations recorded on the issues.

# Human acceptance

- Visual/gameplay review: none captured by this reviewer. The white Mace active-pose visual remains unproven; no screenshot exists for an accepted active tick.

# Agent handoff context

- Owner: `melee-arcade-fix` (issue-503f3e410c01).
- Next exact step: re-run one pinned Arcade/NPC scenario without the client launching its own GUI server (or record the joined server in `run.json`), require the acceptance evidence in the run's pinned server journal, 0 future_contact, and >=1 accepted active Mace with predicted==accepted; then add the tint-transition StructuredLogger event (issue-89efbbc86535) or record human visual acceptance.
- What another agent should not redo: do not attach g742 evidence from the client-spawned dust2cyberiav4 server as the pinned proof; do not mark the tasks verified from this review; do not refile the white-pose gap (already proposed on issue-89efbbc86535) or the game_generation gap (already proposed on issue-b396205aaecf).

# Automatic evidence

- Git status: shared dirty tree preserved; this session added one issue review, one issue suggestion, coordination DB changes, and this changelog
- Build identity inspected: EXE `2026-11-26T22:52:33.481041Z:cb72ee40...d781c` revision 7176d28a; game DLL `build/game/generation-742/game-g742.dll` sha256 `02bd9fa3...`; no build requested
- Journal paths checked: `logs/2026-11-27/20261127_001718/client-01/events.jsonl`, `logs/2026-11-27/20261127_000428/client-01/events-0001.jsonl`, `logs/2026-11-27/20261127_000428/server-01/events.jsonl`, both `run.json` files, and the two `dev-loop/events.jsonl` files

# How to reverse

- Remove only the appended review (id 503) text from the `issue-503f3e410c01` doc; do not rewrite prior text.
- Remove only the appended process proposal text from the `issue-b396205aaecf` doc.
- Restore `.dev/agent-coord/brain.db` from backup if the review/suggestion are withdrawn.
- Delete this changelog. No commit to revert.
