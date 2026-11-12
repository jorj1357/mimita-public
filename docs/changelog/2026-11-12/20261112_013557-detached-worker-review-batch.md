# Detached worker review batch 1

- UTC: 2026-11-12T01:35:57Z
- Local timestamp: 2026-11-11 20:35:57 -05:00
- Branch: unavailable; git status/branch inspection remains blocked by external/libjuice submodule access. Existing edits were preserved; this turn changed documentation only.
- Scope: review eight active EXE-owned detached-worker source sites against ownership, state, async lifetime, external resources, current ABI, and unload disposition.
- Exact changes: created docs/handoffs/cpp-lifetime-batch-detached-20261112.md with revision hashes and per-source blocker findings; linked it from docs/handoffs/cpp-source-inventory.md. No source code or inventory status fields were changed.
- Findings: all eight are current EXE sources and have at least one unload blocker or unresolved async/resource owner. None is certified GAME_DLL_READY. Atomic-ready checks mean some player/avatar result resets happen only after worker completion; the report does not label those specific normal paths as races. Detached execution still lacks a module-unload join/lease.
- Documents read: docs/ROUTER.md; docs/specs/dev-loop/agent-lanes-build-authority.md; docs/specs/hotreload/hotreload.md; docs/skills/spec-behavior-review-v1.md; docs/handoffs/cpp-source-inventory.md; docs/architecture/time-and-formatting/time-and-formatting.md; docs/skills/documentation-checker-v1.md; docs/operations/task-completion/task-completion.md.
- Validation: checked all eight inventory hashes/owners and manually inspected cited implementations, owner headers, and call sites. CSV remains unchanged and correctly reports NOT_SEMANTICALLY_REVIEWED because this blocker-focused batch did not close all per-file fields. No build, tests, or runtime launch.
- Result: PASS_WITH_HUMAN_REVIEW for this blocker-focused batch; full semantic review remains open.
- Human review: none required before continuing the review; confirm any source hash changes before carrying these findings forward.
