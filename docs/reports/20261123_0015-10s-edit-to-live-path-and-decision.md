# 10-second edit-to-live path and decision

**Date:** 2026-11-23 UTC
**Issue:** `issue-136d4553ff92` (high-priority context `ctx-2c2632613a61`)
**Run examined:** `20261122_231306` (Juggernaut Horde, generation 546)
**Scope:** read-only measurement and planning. No compiler, build worker, game
process, or C++ source was changed or started by this work.

## Question

Can edit-to-live C++ latency reach 10 seconds or less under a live
Juggernaut/AI resource-pressure workload, and if so where is the cheapest
correct path?

## Measured floor (real attempts)

| Attempt | Condition | Total | Changed TU | Identity TU | Scan | Full DLL link | Publish |
|---|---|---:|---:|---:|---:|---:|---:|
| `game-dll-20261122T224135400285Z-21907f` (gen 540) | controlled one-file leaf, PCH-on | 38.8 s | 20.07 s | 1.54 s | 1.79 s | 8.17 s | 0.38 s |
| `game-dll-20261122T231534497789Z-06de62` (gen 545) | thin leaf, PCH-off | 40.3 s | 22.54 s | 0.68 s | 1.78 s | 8.00 s | 0.46 s |
| `game-dll-20261122T235222465818Z-9bfbf7` (gen 546) | 2 live MiMITA, heavy pressure | 267.2 s | 151.16 s | 38.58 s | 1.62 s | 59.71 s | 0.49 s |

Gen 540 also carried ~6.17 s of pre-worker work (codegen, manifest, source
snapshot) before the worker stages above; that cost is not in the other rows.

A best-case thin leaf (about 1 s to compile, PCH-off, low pressure) still adds
up to roughly **19 s** on the current path:

```
pre-worker 6.17 + changed TU 1.0 + identity 1.54 + scan 1.79 + full link 8.17
+ publish 0.38  =  about 19.0 s
```

Under the gen 546 pressure sample the full DLL link alone was **59.7 s**, so the
10-second target is not reachable by scan/PCH/caching tweaks while the whole
642-object DLL is relinked on every change.

### Pressure sample during gen 546 (2026-11-23T00:13:04Z, 6 s)

- Host: 12 logical CPUs, 15.65 GB RAM, **1.36 GB free**.
- Server `pid 29536`: +20.84 CPU-s = 347% of one core = **28.9% of the machine**;
  working set 1007 MB.
- Client `pid 17508`: +25.77 CPU-s = 429% of one core = **35.8% of the machine**;
  working set 270 MB.
- Combined game pair ≈ **65% of the machine** while a build needs all cores.
- Live mode: `jugger_horde`, generation 546, **67 NPCs**, 1 player, DLL path,
  286,240 server calls, 0 hotness violations (server journal, same run).
- No build was active during the sample; inference that this pair was live during
  the gen 546 build (started 23:52:22Z) is supported by process uptime but not
  per-stage CPU-attributed. AV/disk contention is unknown.

Full pressure detail is handed to `issue-ed3ac775283d` (CPU owner).

## Verdict

**10 s or less is not achievable on the current monolithic whole-game DLL path.**
The irreducible per-build full link (8 s low pressure, ~60 s high pressure) plus
pre-worker fixed costs already exceed the budget before any compile. Reaching
10 s requires a structural change, not more micro-optimization.

## Three viable paths (all need human approval)

1. **Coarse hot-behavior module DLL (recommended first slice).**
   `ModuleHost` still contains the standalone catalog/build/load path
   (`reloadCatalogIfChanged`, `requestBuild`, `loadCurrent`, worker `poll`), but
   `loadLocalModules` currently sets `mLocalModules = true` and resolves the eight
   coarse modules out of the active whole-game DLL. Restoring a minimal
   `config/modules/modules.json` with **one** coarse behavior module (for example
   `npc-behavior`, whose export and event set already exist) lets the root
   dev-loop build and publish a small module DLL. Expected save-to-active:
   module TU 1-3 s + small-DLL link <1 s + publish <0.5 s + same-process
   activation <0.1 s ⇒ roughly **2-5 s**, including under gen 546-level pressure.
   Keep the MinGW whole-game DLL as the last-good fallback. This is a Cutover-1
   contract expansion and is explicitly a "separately approved evolution" in
   `docs/specs/hotreload/hot-layer-latency-and-language.md`, so it needs approval.
2. **Incremental/patch link experiment.** MSVC `/INCREMENTAL` (persistent `.ilk`)
   or Blink on a compatible MSVC/PDB build, or LLD if incremental support lands.
   Keeps C++ but changes toolchain and cannot reuse the MinGW object cache;
   large, uncertain effort.
3. **Hot behavior layer (script/JIT).** Move the highest-churn behavior out of
   compiled C++. Meets the spec's 50-file p95 ≤1 s target but requires a runtime
   bake-off and is the largest effort.

## Recommended first slice (Option 1)

- One coarse module only (suggest `npc-behavior`), a stable module ABI, and a
  small module include set instead of the 121 MB game PCH.
- Root dev-loop owns the module build request, candidate queue, publication,
  timings, and rollback. `ModuleHost` loads and activates at a safe boundary.
- Unsupported or failed module edits reject the candidate and keep the prior
  generation; no partial application.
- Acceptance (must be repeated, not single-shot):
  - save→activation p95 ≤10 s under measured pressure at 1, 2, and 4 live MiMITA
    processes, with the full stage breakdown;
  - same-PID activation event plus an owner event for the changed behavior and a
    visible behavior change;
  - last-good whole-game DLL retained on failure; no duplicate auto-build of an
    already-published revision;
  - at least ten human-triggered edits, cold and warm.

## Non-architectural waste that can proceed without approval

- Suppress the duplicate automatic build already observed for an
  already-published revision (`game-dll-...0e52c1` re-prepared the same revision).
- Incremental pre-worker manifest/source snapshot (tasks
  `cpp-latency-junction-snapshot`, `dll-snapshot-cause-and-incremental-index`).
- Warm reverse-dependency scan is already near its floor (~1.6 s).
- Reduce build/gameplay contention: two MiMITA processes consume ~65% of the
  machine during builds (owned by `issue-ed3ac775283d`).

## Decision requested

Approve **Option 1 (one coarse behavior module DLL)** to expand the Cutover-1
one-DLL contract for a single behavior module, or authorize **Option 2**
(toolchain experiment), or defer to **Option 3** (runtime bake-off). Until that
decision, the game architecture is left unchanged.
