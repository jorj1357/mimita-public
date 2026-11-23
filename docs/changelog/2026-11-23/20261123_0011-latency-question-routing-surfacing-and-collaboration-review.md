# Latency question routing, hot-path surfacing, and collaboration review

**Date:** 2026-11-23 UTC
**Context:** `ctx-5edab329a16d`
**Issue:** `issue-136d4553ff92`
**Agent:** `horde-targeting-proof`

## What this session did

Answered the user's architecture/latency/CPU/collaboration questions for
`ctx-5edab329a16d` without repeating the two answers already appended to the
issue by `cpp-latency-small-tu-benchmark` and `codex-networking-stall-fix`.
No source code, build script, compiler, process, or game runtime was changed;
no build or test was started.

## Findings recorded

- Confirmed the folded-module architecture in source: `devscripts/game_build_worker.py:49-64`
  defines 10 folded module wrappers plus 1 identity-only source; `src/hot-reload/module-host.cpp:785-874`
  resolves all 10 exports from the active whole-game DLL.
- Confirmed the standalone path is dormant, not deleted: `ModuleHost` still has
  catalog reload/build/load/poll, but `startup` calls `loadLocalModules`
  unconditionally (`:1119`) and `poll()` returns when `mLocalModules` is true
  (`:1155-1158`). `config/modules/modules.json` is absent (removed in `7efe9c17`).
  Therefore a coarse module is a re-enable/harden task on existing machinery.
- Confirmed the per-generation wrapper fanout appears fixed: `identity_text()` is
  byte-stable (`game_build_worker.py:776-822`) and only one identity-provider TU
  regenerates. Generation 545 compiled the changed source, the PCH-excluded
  `npc-world-query.cpp`, and the identity provider, then skipped 640 objects.
- The remaining latency ceiling is the changed-TU compile under load plus the
  monolithic LLD link (Gen545 7.949 s quiet, about 25.7-63 s loaded; 644 inputs,
  483 MB object bytes, 226 MB DLL).
- Duplicate-wake evidence: the same context was recorded into at least seven
  session-specific context ids and answered by multiple agents.

## Proposals recorded

- A read-only `hot-path-probe` that maps a source path/owner to standalone hot
  module, folded whole-game DLL, or resident/EXE, with link input count/bytes and
  code references, so the architecture question is answered in one command.
- Keep and formalize the existing seven-step discovery recipe, but classify the
  edit first: single small behavior TU versus many-file C++ edit.
- Collaboration waste reductions: dedupe high-priority context fan-out, group
  wait/build-friction issues by signature, enforce edit leases at the build-input
  snapshot, add a per-goal verified-truth digest, and aim the existing review
  scheduler at unproven/stale proof.

## Evidence

Full analysis appended to
`docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md`
(suggestion by `horde-targeting-proof` at 2026-11-23T00:11:34.982Z).
