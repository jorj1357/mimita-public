# Independent bounded review of issue-5d82fc58a875

- Task ID: wake-7d23f1f7644f
- Agent: issue-review-sweeper
- Focus: one read-only review of the late-join live team identity bug (`issue-5d82fc58a875`), its readiness, proof status, linked artifacts, and the latest relevant journals.
- Summary: Appended one independent review (review id 378, verdict `needs-evidence`) to `issue-5d82fc58a875`, one retention evidence confirmation to `issue-3f776ce60511`, one build-resource gap suggestion to `issue-ed3ac775283d`, and one issue-linked question (`msg-45fcacb66012`) to `root-resource-loop`. No source edit, build request, test run, close, owner change, or fix claim.
- Status: complete (review only)
- Date, time, timezone: 2026-11-24T19:48Z, ISO 8601

# Pre-existing changes

- The shared checkout retains many dirty/untracked entries from other agents/sessions (`config/*`, `src/combat`, `src/network`, `src/renders`/`src/ragdoll`, `src/vehicles`, `shaders`, several `docs/issues`, `docs/specs`, and `docs/changelog`). This session modified only `docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-5d82fc58a875.md`, `docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md`, `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md`, the coordination DB, and this changelog. No compiler, `build.py`, module worker, second dev-loop, or runtime test run was invoked.

# Requested behavior

Run one bounded independent review of `issue-5d82fc58a875`: inspect issue brief/readiness, proof status, linked artifacts, and the latest relevant journals; append one evidence-grounded review with observed facts, inference/uncertainty, contrary evidence, and a next discriminating check; then a short development-process review with one specific proposal to the best existing improvement issue (or none if unsupported). Do not edit source, build, close, or claim a fix.

# Specification alignment

- AGENTS.md gates 0b/0c/0d, 2, 3, 4: coordination begin/sync/observe; observation separated from inference; no compiler/build/second dev-loop in a review wake.
- `docs/specs/dev-loop/agent-proof-and-verification.md`: a build or a prior journal claim is not current behavior proof; acceptance requires a pinned run journal showing the required events at the shipped generation.
- `docs/specs/gamemodes/teamdeathmatch.md`: live team assignment, roster capacity, and friendly/enemy identity are the expected runtime behavior.

# Exact implementation changes

None. No source, specification, or configuration change.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/bug/20261123-late-join-to-a-live-team-match-has-no-team-no-ho-issue-5d82fc58a875.md` | one pass review (id 290) | + review id 378 (`needs-evidence`) | required independent review record | remove only review id 378 |
| `docs/issues/improvement/20261120-measure-storage-usage-and-keep-log-retention-bou-issue-3f776ce60511.md` | prior retention review | + confirmation/evidence suggestion | observed retention risk materialized | remove only the appended suggestion |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` | prior resource evidence | + build-resource gap suggestion (`ab797d`) | previously unrecorded gap_key | remove only the appended suggestion |
| `.dev/agent-coord/brain.db` | no review id 378 | review 378 + two suggestions + message `msg-45fcacb66012` | coordination record | restore from backup if withdrawn |
| `docs/changelog/2026-11-24/20261124_1948-issue-5d82fc58a875-independent-review.md` | absent | new changelog | one changelog per repository-touching session | delete file |

# Contract / ABI changes

None.

# State / schema changes

- `issue-5d82fc58a875` remains `in_review`; owner `latejoin-team-identity`; 3 tasks still open with empty task evidence.
- `issue-3f776ce60511` gained one evidence suggestion in its discussion.
- `issue-ed3ac775283d` gained one build-resource gap suggestion and one issue-linked message (`msg-45fcacb66012` to `root-resource-loop`).

# Diagnostics

- Readiness: `ready=true`, score 8/8, `state=in_review`, owner `latejoin-team-identity`, 3 tasks open (`latejoin-team-capacity-spawn`, `latejoin-team-identity-server`, `latejoin-team-outlines`).
- Proof rows for this issue are not verified: id74 (`task-proof-39dbdfdf1222`) unproven (journal predicate not found); id75 (`task-proof-c63791220bc7`) unproven; id76 (`task-proof-8ee0e191b3dd`) stale (source changed); id77 (`task-proof-e54f24bee063`) stale; id78 (`task-proof-72fdb2cb02ab`) stale. Global proof status: proven=3, stale=27.
- Cited journals `logs/2026-11-23/20261123_{165703,191110}/...` are absent. `logs/` currently holds 2026-11-24 roots plus legacy `MM-DD-YYYY` dirs; no `2026-11-23` and no `11-23-2026` directory. Only a non-journal leftover exists: `.dev/agent-command-20261123_165703-client-02.jsonl`.
- Current source hashes differ from proof-recorded hashes: `src/network/server-gamemode.cpp` `13ef9af0...` (recorded `adf0ae64...`); `src/render/render-player.cpp` `bdead54b...` (recorded `e49a16f7...`); `src/network/server-gamemode.h` `00eeddc1...` unchanged. Last commit touching `server-gamemode.cpp` is `51306c08` (2026-11-24T05:22Z) for unrelated NPC/RL work.
- Code inspection: `chooseLiveJoinTeam`/`applyLiveJoinIdentity` at `src/network/server-gamemode.cpp:2766-2884` still emit `match.actor.late-join-assigned` and `match.actor.late-join-spawn`; the live-join branch is reached at `:2904-2919`; `gamemode.spawn-new-actor` is emitted at `:2934`; `src/render/render-player.cpp:604-624` gates team layers on `teamIdentityKnown`.
- No retained `2026-11-24` `events*.jsonl` file contains `late-join-assigned`, `late-join-spawn`, or `team-roster-npc.replaced` (0 matches).
- New build-resource gap not previously in docs: `build-resource-sampling:game-dll-20261124T193005958527Z-ab797d` (21.783s, gen606, 22 samples, mean host CPU 86.779%, minimum available RAM 3,192,066,048 B, `same_run_server_client=true`, client mode `stale_or_unreported`; missing matched prebuild baseline, host commit charge/limit, client-mode confirmation).

# Validation

- `agent-coord.py begin/sync/observe/board`, `proof list/status`, `issue readiness` (via observe), `issue brief` for the retention and runtime-registry issues, and a read-only DB message query.
- Direct filesystem checks for the cited runs and `logs/` roots; SHA-256 of the current source files; source reads of the late-join and outline paths.
- `issue review` appended review id 378; `issue suggest` appended two suggestions; `send --issue` posted one question (`msg-45fcacb66012`).
- No compiler, build request, module worker, second dev-loop, or runtime test run was invoked.

# Findings

1. The issue's runtime proof is not currently verifiable: 2 proof rows are unproven and 3 are stale, and every cited `2026-11-23` journal has been removed from `logs/`. This is missing evidence, not a demonstrated failure.
2. The behavior implementation persists in current source, including `gamemode.spawn-new-actor`, so the earlier missing-event objection is addressed in code, but not re-proven at runtime.
3. No retained `2026-11-24` run exercises a late join, so no current-generation runtime evidence exists.
4. The retention risk previously predicted on `issue-3f776ce60511` has now occurred: both `logs/2026-11-23` and `logs/11-23-2026` are gone.
5. One new build-resource gap (`...-ab797d`) was recorded; two sibling gaps from the same brain_check were already documented, so they were not re-appended.

# Confidence

High on readiness, proof-row results, journal absence, source hashes, git history, and the code presence of the late-join/outline paths (direct reads and filesystem checks). Moderate on the inference that the behavior still works (code persists; no current run). Unknown: whether unrelated later edits regressed the behavior, and whether the required events would still fire on generation 606+.

# Proof

No proof claim. Read-only review; evidence is source-tier, filesystem-tier, and coordination-tier. No behavior change introduced.

# Regression review

- Regression entry appended: no
- Why: no confirmed code change or fix; the proof is stale and its journals are gone, which is unverifiable, not a confirmed regression.

# Human acceptance

- Visual, gameplay, multiplayer review: none (not applicable)
- Still unverified: late-join team assignment, team-spawn placement, roster capacity replacement, enemy NPC targeting/damage on the joiner, and friendly/enemy outline colors at the current generation; FFA/teamless non-assignment remains code-inspected only.

# Agent handoff context

- Owner: `latejoin-team-identity`. Next exact step: on the current dev-loop generation, run one isolated TDM two-client late-join (test profile `tdm-validation`, launch mode 41; second client after `network.mode.start` `stage=active`), copy and hash the server-01 and client-02 `events.jsonl` into retained artifacts, then verify and re-record proof against the current source hashes.
- What another agent should not redo: do not treat review id 290 or the gen565/gen569 claims as currently verifiable; do not cite the purged `2026-11-23` journals as proof; do not close the issue on existing records.
- Process follow-up lives on `issue-3f776ce60511` (proof/journal retention, confirmed) and `issue-ed3ac775283d` (build-resource gap `...-ab797d`).

# Automatic evidence

- Git status: shared dirty tree preserved; this session added two issue suggestions, one review, one message, coordination DB changes, and this changelog
- Build identity: latest published game-DLL generation observed 606; a gen607 attempt `game-dll-20261124T193404565977Z-41251a` was reported in the brain check; no build requested
- Journal paths checked: `logs/2026-11-23/20261123_165703/`, `logs/2026-11-23/20261123_191110/` (absent), all retained `logs/2026-11-24/**/events*.jsonl`, and `.dev/build-timings/game-dll-20261124T193005958527Z-ab797d.resource.json`

# How to reverse

- Remove only review id 378 from the `issue-5d82fc58a875` doc; do not rewrite prior text.
- Remove only the appended suggestions from the `issue-3f776ce60511` and `issue-ed3ac775283d` docs.
- Restore `.dev/agent-coord/brain.db` from backup if the review/message is withdrawn.
- Delete this changelog. No commit to revert.
