# 10 s edit-to-live: measured verdict and coarse-module path

Date: 2026-11-23

## Goal

Refocus on the high-priority request in `ctx-2c2632613a61`: get C++ edit-to-live
to 10 seconds or less under measured live Juggernaut/AI resource pressure, then
re-prove it under more pressure (multiple MiMITA processes). Preserve correct
caching, stable module identity, the MinGW whole-game DLL fallback, and one root
dev-loop build authority.

## Verdict

Using real builds gen 540 (`21907f`, controlled one-file leaf), gen 545
(`06de62`, PCH-off thin leaf), and gen 546 (`9bfbf7`, under two live MiMITA), the
current monolithic path has a low-pressure floor near **19 s** even with a ~1 s
thin compile: pre-worker ~6.2 s + compile ~1 s + identity ~1.5 s + scan ~1.8 s +
full DLL link ~8.2 s + publish ~0.4 s. Under gen 546 pressure the full link
alone was **59.7 s**. **10 s is not reachable by caching/scan/PCH tweaks while the
642-object whole-game DLL is relinked on every change.**

## Plan

Recommended first slice: restore the existing `ModuleHost` standalone-module path
for **one** coarse behavior module (for example `npc-behavior`) so the root
dev-loop builds and publishes a small module DLL (~2-5 s expected),
hot-loads it in the live Juggernaut process, and keeps the whole-game MinGW DLL
as the last-good fallback. Toolchain incremental-link and hot-behavior-layer
alternatives are recorded but larger. This expands the approved Cutover-1
one-DLL contract, so it is submitted for human decision rather than started.

Decision-ready detail: `docs/reports/20261123_0015-10s-edit-to-live-path-and-decision.md`.

## Resource pressure handoff

Direct sample 2026-11-23T00:13:04Z (6 s): 12 logical CPUs, 15.65 GB RAM,
1.36 GB free; server `pid 29536` 347% of one core (28.9% of machine, 1007 MB) and
client `pid 17508` 429% of one core (35.8% of machine, 270 MB), together ≈65% of
the machine. Live mode `jugger_horde`, generation 546, 67 NPCs, 1 player, 0
hotness violations. Appended to `issue-ed3ac775283d` for the CPU owner; the
build-latency verdict was appended to `issue-136d4553ff92`.

## Scope and verification

No compiler, build worker, game process, C++ source, or dev-loop configuration was
changed or started. Evidence is from existing `.dev/build-timings` summaries, the
live run journal, and one read-only process sample. Build success and runtime
activation are reported as separate claims; no visible behavior change was made
or claimed.
