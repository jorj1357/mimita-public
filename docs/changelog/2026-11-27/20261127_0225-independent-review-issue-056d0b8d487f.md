# issue-056d0b8d487f Independent Issue Review

# Task

- Task ID: wake wake-e82e9445be7b, context ctx-fb40e6c7c3b9 (map editor block removal vs server collision)
- Owner: rocket-league-ball
- Focus: verify the block-only authoritative fast path and local player+NPC parity for the reported editor/server collision divergence
- Summary: Read-only verification of pinned run 20261127_021425 (game generation 755). Appended one issue review. No source edit, no build, no runtime started, no task claimed, nothing closed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-27T02:25Z

# Pre-existing changes

- The shared checkout already contained many other agents' uncommitted edits. This session edited no source and reverted nothing.
- Other agents already worked this context: map-authored-parity created canonical issue-056d0b8d487f (+ duplicate issue-e026c624fc22), rocket-league-input-fix recorded/verified evidence-6a0ad38d26f1, dashboard-fps-relaunch appended a suggestion and a message.
- Files created by this session: this changelog and an append-only issue review written through `agent-coord.py issue review`.

# Requested behavior

- User report: a block placed then removed in the map editor still collides on the server; what the editor shows must equal what players and NPCs collide with, and NPC parity must equal player parity, with no shared-header fanout.

# Findings

- Verified delete-to-authority = ~1.07s. In server-01/events.jsonl: add applied 02:15:06.566Z (triangles 9484, geometry_revision 1, authored_ms 299.97, npc_ms 428.68, duration 728.68) and delete applied 02:15:15.066Z (triangles 9472, geometry_revision 2, authored_ms 101.46, npc_ms 899.25, duration 1000.72). Client delete was 02:15:14.000Z.
- Zero block-delta.failed, zero rebuild.discarded, and zero signature-queued events in that server journal. This matches the issue claims for acceptance 2, 3, 5.
- Source parity cleared: `.dev/build-timings/game-dll-20261127T021242231192Z-14ccc6.json` shows generation 755, code_hash e848c266..., completed 02:13:33Z, compiling the fast-path files after the final `authored-block-rebuild.h` edit. The 01:43:27Z id in run.json is the EXE artifact only, so no re-pin is needed for these files.
- Acceptance 1 is not literally met: "next eligible tick / <=1-2 ticks" is ~64 ticks in practice; the real bound is the ~1s coalesce + NPC rebuild.
- Acceptance 6 is the remaining stall: npc_ms (899.25 delete) dominates and comes from `buildNpcWorldCollisionDetached` at `src/network/server-gamemode.cpp:8496` inside `applyAuthoredBlockDeltaNow` rebuilding the whole NPC mirror.
- The "NPC parity == player parity" half is unproven: the run is one host with a local client (run.json server args show `--npcs 5`); no remote second client was sampled, so RTT-bounded client parity is unknown.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: coordination sync/ack, build evidence kept separate from runtime proof, no source/build change, no duplicate issue created.
- No shared header was touched by the reviewed fix (`world.h`/`server.h` unchanged); matched the user's "no fanout" instruction.

# Exact implementation changes

- None. Append-only issue review + this changelog only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop build request, or second loop invoked.
- Commands used: `agent-coord.py begin/sync/inbox/board/context ack/issue show/issue review/activity set`, read-only journal search over server-01 and client-01 events.jsonl, and read-only inspection of run.json and build-timings JSON.
- Review artifact: issue-056d0b8d487f review, verdict `reviewed-evidence-confirmed; local-authority-parity-proven; remote-client-parity-and-npc-stall-open`, 2026-11-27T02:2xZ.

# Measured evidence

- Run: 20261127_021425, game generation 755, game_dll_sha256 e848c2669491e61d922bcfaf8d1026bc88b66cc4b5cb4357bc35dcd8d434eb6a.
- Add apply duration 728.68ms; delete apply duration 1000.72ms; delete-to-authority ~1.07s.
- Journal counts: 2 block-delta.applied, 0 block-delta.failed, 0 rebuild.discarded.

# Regression review

- Regression entry appended: no. No behavior or code change was made.

# Human acceptance

- Visual/gameplay walk-through: not performed.
- Still unverified: remote-client (second player) RTT parity; incremental NPC-mirror patch for acceptance 6; literal 1-2 tick authority latency.

# Agent handoff context

- Next steps left unowned: (1) acceptance 6 — patch only the affected block triangles/broadphase cells instead of rebuilding the whole NPC mirror; (2) add a remote-client parity run and pin its journals.
- Duplicate issue-e026c624fc22 is correctly `needs_clarification`; close it as a duplicate after the canonical record is accepted.
- Do not re-implement the block-only fast path; it is present and runtime-proven for the local case.
