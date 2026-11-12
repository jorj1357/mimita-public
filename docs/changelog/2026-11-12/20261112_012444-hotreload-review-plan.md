# Hot-reload source review and multi-DLL plan

- UTC: 2026-11-12T01:24:44Z
- Local timestamp: 2026-11-11 20:24:44 -05:00
- Branch: unavailable; prior git status/branch inspection was blocked by the external/libjuice submodule access error. No source changes were made in this turn.
- Scope: document the complete per-source evidence gate for assigning active game C++ implementations to hot reload, and specify when/how multiple DLLs may be introduced.
- Exact changes: added “Phase 0 semantic-review evidence gate — every C++ source” and “Later multi-DLL design — defer until one DLL is proven” to docs/specs/hotreload/hotreload.md immediately before Phase 1.
- Reason: the repository inventory is lexical/build mapping, not proof of state/resource ownership or safe unload. The spec now requires evidence for build ownership, dependencies, persistent state, callbacks/jobs/threads, external resources, ABI crossings, and a final evidence-backed disposition for every row. It also defines a managed, versioned, dependency-checked multi-module path, deferred until one full game DLL is proven.
- Documents read: docs/ROUTER.md; docs/specs/hotreload/hotreload.md; docs/architecture/live-development/live-development.md; docs/architecture/live-development/hot-kernel.md; docs/architecture/time-and-formatting/time-and-formatting.md; docs/skills/documentation-checker-v1.md; docs/operations/task-completion/task-completion.md.
- Validation: checked insertion marker and re-read the updated spec section. Documentation-only change; no build, test, or runtime launch.
- Result: PASS_WITH_HUMAN_REVIEW. The plan does not itself complete the 632-source semantic review or implement runtime quiescence. Existing inventory statuses remain unresolved until each source is traced and the rows are updated.
- Human review: confirm the strict no-unresolved-row cutover gate and one-DLL-first policy.
