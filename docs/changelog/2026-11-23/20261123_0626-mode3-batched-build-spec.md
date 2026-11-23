# Document Mode 3 batched coordinated builds

**Date:** 2026-11-23 UTC
**Context:** `ctx-c4fd98cf6c3c` (related `ctx-ab0aec3f6cc8`, `ctx-0a44d8703129`, `ctx-740583582d02`)
**Issue:** `issue-136d4553ff92` (cross-linked `issue-ed3ac775283d`)
**Agent:** `cpu-profile-to-fix`

## User request

Write the current build-mode discussion and the proposed Mode 3 batch build into
the appropriate spec, explaining what it is and what we want.

## Where the modes live today

- `docs/specs/dev-loop/agent-lanes-build-authority.md` documents the active
  Mode 1 shared-checkout policy and the retired historical Mode 2 lanes.
- `docs/specs/dev-loop/dev-loop.md` §5 defines the current trigger: a saved
  watched compiler input automatically queues an incremental build after a short
  debounce. That is the "save = build" behavior Mode 3 changes.
- `docs/specs/dev-loop/agent-build-queue.md` defines build requests, coalescing,
  rotating build ownership, and resource-aware concurrency.

## Change

Added a new **"Mode 3 — batched coordinated builds (target)"** section to
`agent-lanes-build-authority.md`, marked proposed/not implemented. It records:

- save does not build; edits only bump the source generation and staleness;
- agents declare edits ready and agree on one build window;
- the root dev-loop runs exactly one focused, fully-resourced, bounded build;
- build processes drain before agents test pinned runtimes and capture
  `events.jsonl`;
- repeat until each change is proven;
- invariants (single builder, cache/module identity, MinGW fallback, last-good
  retention, pinned proof, no indefinite agent blocking);
- proof/measurement (stage timings, whole-PC CPU/RAM between and during windows,
  build/process count, edit-to-visible latency, Mode 1 baseline comparison);
- acceptance criteria and open questions;
- relationship to `dev-loop.md`, `agent-build-queue.md`, and
  `build-time-observability.md`.

## Scope and verification

Documentation only. No source, compiler, build, dev-loop, or runtime process was
invoked or changed. This changelog records source (spec text) evidence only; it
is not runtime or human-acceptance proof of Mode 3 behavior. Mode 3 remains a
target until the user approves and the build trigger policy and guard are
implemented and measured against the Mode 1 baseline.
