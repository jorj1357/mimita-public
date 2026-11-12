# Begin all-source lifetime review

Started the all-eligible-source lifetime-review step for the requested whole-game DLL scope. The work is an exhaustive automated triage pass; it does not claim semantic ownership or safe-unload proof.

- Reused the 731-file inventory and filtered 614 direct canonical game-build sources plus 18 saved-manifest-only Manifold sources.
- Extended the inventory CSV with per-source triage bucket, mutable/static candidate counts, automated scan status, semantic review status, and explicit state-owner, callback/job, external-resource, unload-evidence, and cutover-group fields.
- For the 632 in-scope files, triage found 278 with mutable static-storage candidates, 58 with callback/thread/job candidates, 25 with unresolved static declaration scope, and 271 with no lexical lifetime hit. Buckets are exclusive per file; candidate counts can include false positives.
- Preserved all 632 semantic review statuses as `NOT_SEMANTICALLY_REVIEWED`; state ownership and safe unload remain unproven for them. Current reviewed anchors remain dedicated `runServer` stack state, caller-owned `ListenServerState` and thread, `NpcSystem`/NPC records, server-owned `ServerNpc` projections, `gpMpContext`, `gpWorld`, and EXE-owned `HotReloadSystem` memory.
- Added `docs/handoffs/cpp-lifetime-review.md` with manually traced server/audio/async-worker/callback findings and eight detached-thread sites requiring explicit generation-lifetime handling. These are actionable blockers, not safe-unload approvals.
- Read `docs/skills/spec-behavior-review-v1.md`, `docs/skills/documentation-checker-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and the routed live-development/hot-reload/build-authority documents.

Validation: parsed the scanner with Python AST, parsed the JSON, regenerated the CSV from the existing saved inventory, and confirmed all 731 unique rows, the 614/18 scope split, and explicit pending semantic status. No compiler, build, test, or runtime was invoked. A relevant existing documentation TODO remains at `docs/architecture/player-npc-systems/npc-movement.md:22` (`9 8 2026 1019 est jorj todo - explain`); suggested replacement: explain the current NPC movement and stuck-recovery owner and the runtime evidence needed to evaluate it. It was not changed.
