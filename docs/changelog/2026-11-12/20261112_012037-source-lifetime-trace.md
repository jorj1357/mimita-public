# Source lifetime trace

- UTC: 2026-11-12T01:21:00Z
- Local timestamp: 2026-11-11 20:21:00 -05:00
- Branch: unavailable; git status and branch inspection are blocked by the external/libjuice submodule refusing access (`fatal: cannot chdir to .../.git/modules/external/libjuice: Permission denied`). No source changes existed before this documentation edit as far as the available view showed.
- Scope: extend docs/handoffs/cpp-lifetime-review.md with traced process/session state, worker/job/callback lifetime, renderer/device and network ownership evidence.
- Exact changes: appended “Additional source-traced owners (2026-11-12)” to docs/handoffs/cpp-lifetime-review.md. No source code, build ownership, loader, or runtime behavior changed.
- Reason: hot-reload correctness requires proving who owns persistent state/resources and that old generation code cannot be reached by worker threads or stored callbacks after unload.
- Documents read: docs/ROUTER.md; docs/specs/dev-loop/agent-lanes-build-authority.md; docs/architecture/live-development/live-development.md; docs/architecture/live-development/hot-kernel.md; docs/specs/hotreload/hotreload.md; docs/architecture/time-and-formatting/time-and-formatting.md; docs/operations/task-completion/task-completion.md; docs/skills/documentation-checker-v1.md.
- Validation: manually inspected the cited source ranges and searched callback/thread/graphics allocation symbols. git status could not complete because the external/libjuice submodule path is denied. No build, test, or runtime launch, per build authority and because this is a static ownership trace.
- Result: PASS_WITH_HUMAN_REVIEW for this targeted trace. Unresolved: complete per-file semantic review of all eligible files and verification of libjuice callback-drain guarantees, callback unregister paths, detached work, all graphics-resource owners, and runtime activation/quiescence. This report does not certify any source as unload-safe.
- Human review: review the appended evidence and decide whether the proposed resident owner / generation quiescence contract is the desired boundary.
