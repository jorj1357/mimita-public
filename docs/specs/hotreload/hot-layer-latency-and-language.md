// 2026-11-19
/* purpose
* define the deferred hot-layer latency and language target for 50 changed files
* separate hot behavior logic from native mechanisms and coarse native modules
* this file DOES NOT change the Cutover-1 one-game.dll contract
* this file DOES NOT relax the hotness proof rules
* this file DOES NOT claim a current implementation or current status
*/

# Hot-layer latency and language target (deferred)

## Status and authority

This specification is a **deferred target**, not a Cutover-1 requirement and
not a description of current implementation. It records the architecture needed
if normal iteration must absorb **50 changed files in one build at one second or
less** on the reference laptop.

It is subordinate to:

- [`hotreload.md`](hotreload.md) for Cutover-1 behavior, hotness proof, and
  evidence rules;
- `build/source-manifest.json` and `config/source-owners.json` for current source
  ownership;
- [`../dev-loop/build-time-observability.md`](../dev-loop/build-time-observability.md)
  and [`../dev-loop/agent-lanes-build-authority.md`](../dev-loop/agent-lanes-build-authority.md)
  for build measurement and build authority.

Cutover-1 remains **one replaceable game DLL**. This document does not change
that contract. It defines a later, separately approved evolution.

Human decisions recorded for this target:

- actual C++ syntax is **not mandatory** for all hot behavior;
- a managed runtime such as C# is **acceptable**;
- **language power is prioritized over minimal language size**;
- the reference machine is the current 8-core / 12-thread laptop;
- implementation is deferred; no build-time or hot-reload work is authorized by
  this document alone.

## Problem statement

A native C++ build of 50 changed translation units cannot meet one second on the
reference machine. Reference measurements from 2026-11-19:

| Evidence | Value |
|---|---|
| Warm no-change worker | 0.29-0.40 s |
| 12 changed TUs | 42.8 s compile + 16.5 s link |
| 43 changed TUs | 162.7 s compile + 36.4 s link |
| Zero-compile monolith link | 12-36 s |
| PCH / DLL / object set | 121 MB / 239 MB / 1.27 GB |
| CPU | 8 cores / 12 threads |

With 50 changed TUs and 12 threads, a one-second budget allows roughly 0.24 s
per TU including any link and activation. Current per-TU compile is 10-90 s.
Splitting the DLL reduces link time but does not reduce the 50 compiles. A
per-file DLL design still compiles 50 files and adds loader, ABI, and static
state costs.

Therefore the one-second many-file target requires that the changed files do
not pass through a C++ compiler and DLL link on save.

## Required architecture

```text
resident native kernel
  process, device, OS/input/network primitives, storage, ECS storage,
  physics/collision solvers, loader, logging sink

hot behavior layer (JIT or bytecode runtime)
  AI perception/targeting/decisions, combat and weapon policy,
  gamemode/spawn policy, movement policy, UI flow, network policy and
  validation, replay/presentation policy, tuning, authored rules

coarse native modules (8-16, only where measured performance requires them)
  small stable-capability DLLs; one changed module links at a time
```

The hot layer must contain the **implementation of behavior**, not merely
bindings that call slow-to-change C++ functions. Native code owns mechanisms and
validated primitives. The hot layer owns decisions, policy, state machines,
rules, and presentation logic.

A wrapper-only design does not satisfy this specification: if fixing a behavior
bug still requires editing a compiled C++ function that the wrapper calls, the
design has not met the intent.

## Ownership matrix

Hot-owned logic includes:

- NPC perception, memory, belief, targeting, navigation choice, combat intent,
  and lifecycle;
- weapons, projectiles, damage, knockback, effects, and ragdoll policy;
- gamemode rules, team/role assignment, spawn policy, and progression;
- movement and vehicle policy above collision/query primitives;
- game UI, HUD, menus, terminal commands, and tools;
- render policy, draw selection, uniforms, upload decisions, and presentation;
- replay policy and authored presentation;
- packet IDs, schemas, wire layouts, encode/decode paths, handlers, and
  validation rules;
- join/reconnect, session health, map sync, spawn authority, lag-compensation
  policy, and snapshot policy.

Native-owned mechanisms include:

- process lifetime and loader/generation barrier;
- window, GL context, device creation, swap, and OS event pump;
- sockets, ICE/UDP, framing/encryption primitives, and timers;
- immutable transport and codec primitives below packet registration; packet
  schemas, layouts, and handlers remain hot as required by `hotreload.md`;
- resident state storage and handles that outlive a generation;
- memory, job, and threading mechanisms;
- physics/collision solvers and world-query primitives whose performance cannot
  be met in the hot layer;
- logging sink and persistence mechanism.

A behavior is not hot because it is scripted or modular. It is hot only when the
real executable journal shows the owner path for that behavior, as required by
[`hotreload.md`](hotreload.md). Scripted, JIT, and native-module paths must emit
the same bounded owner events and must never silently fall back to a resident
duplicate.

## Latency contract

The many-file target is:

- **50 changed hot-layer files**, saved together, from source observation to the
  new behavior active in the same process: **p95 at most 1.000 s** on the
  reference laptop.
- The path must not invoke a C++ compiler or link a game DLL.
- The measurement is reported cold and warm, with file count, bytes, runtime,
  JIT/bytecode state, and p50/p95 over at least ten repeated human-triggered
  runs.
- Stage boundaries are recorded separately: file observation, parse/compile/JIT,
  activation at a safe boundary, first owner event, and first visible result.
- An unsupported edit class is rejected visibly and the previous generation
  remains active. Partial application is forbidden.

Native C++ module changes have a **separate measured budget**. This specification
does not promise 1 s for them. A change to the native kernel or to a coarse
module is reported as its own cold/native build result.

The already-specified warm no-change objective remains at most one second from
worker entry in the existing build system.

## Language policy

Language selection is **power-first**, but no language is selected by preference
alone. The runtime must pass the latency and runtime-performance gates.

Candidates to benchmark:

| Candidate | Role | Primary risk |
|---|---|---|
| C# / .NET hot reload | strongest practical language power; fast edit loop | interop/GC, supported edit class, deployment weight |
| C++ JIT (Cling/LLVM ORC) | actual C++ authoring | 50-file parse/JIT likely exceeds 1 s; ABI/global integration |
| AngelScript with JIT | C++-like game scripting; near-native hot paths | embedding/API registration; not full C++ |
| LuaJIT | fastest dynamic option; low interop overhead | Lua ergonomics; not C++-like |
| TinyCC C JIT | very fast C-like compile/load | C only, low optimization, Windows header limits |
| JSON/DSL | maximum reload speed and simplicity | limited expressiveness |

The bake-off must measure, for 50 representative files:

- save-to-active p95, cold and warm;
- JIT/bytecode compile time and first-tick time;
- steady-state tick cost for representative NPC, combat, and network-policy
  workloads versus the current C++ baseline;
- support for the accepted edit class (method-body only versus structural
  edits), visible rejection of unsupported edits, and rollback;
- Windows x64 support, debugging, deterministic execution options, memory, and
  toolchain reproducibility.

Provisional order, subject to measurement:

1. C# if language power is the dominant goal and its tick budget and hot-reload
   edit class pass;
2. C++ JIT if actual C++ authoring is required and 50 files meet the latency
   gate;
3. AngelScript JIT if C++-like authoring with better in-process performance is
   required;
4. LuaJIT if runtime performance dominates;
5. TinyCC for a fast C-like fallback;
6. JSON/DSL for the subset that does not need general code.

## DLL strategy

- **One file equals one DLL is rejected.** It leaves the 50 compiles unchanged
  and adds loader, module identity, relocation, ABI, and static-state costs.
- Coarse native modules are allowed only in the range of roughly 8-16 stable
  capability modules. A changed module compiles its few translation units and
  links a small DLL; it must not require relinking the 239 MB game DLL.
- Native modules use a minimal include set or a small module-specific PCH, not
  the 121 MB shared game PCH.
- A coarse module remains hot only when the journal proves activation and the
  owner event for the exercised behavior.
- No automatic per-file DLL generation, symbol-level patch DLL, or duplicate
  native path is permitted without a separate specification and measurement.

## Native boundary rules

- The hot layer reaches native code through versioned, bounded capabilities and
  fixed-size events. Do not pass STL containers, vtables, exceptions, or
  generation pointers across the boundary.
- Native primitives validate hot-layer requests and enforce safety limits.
- Hot code cannot own process lifetime, sockets, device creation, or resident
  handles.
- Script/JIT state is versioned with the behavior bundle. Activation preserves or
  rejects state explicitly; it never silently resets a live session.
- Multiplayer requires deterministic hot behavior, server authority for
  gameplay decisions, and a shared bundle hash. A client and server with
  different hot-layer revisions must not agree to play.
- A hot-layer failure rejects the candidate, restores the prior generation, and
  records the failure with the edit class, file, runtime error, and actor/mode
  context.

## Deferred migration gates

No work starts until a human approves the benchmark result and the chosen
runtime.

1. **Native impossibility proof.** Benchmark 50 trivial C++ TUs and 50 real
   module TUs on the reference laptop. Record compile/link/load. This establishes
   the native floor.
2. **Runtime bake-off.** Run the candidate matrix against the 50-file latency and
   tick-performance gates.
3. **Stable boundary prototype.** Embed the winning runtime, expose one bounded
   capability set, and move one real behavior (for example NPC targeting or
   weapon tuning) into the hot layer. Prove same-PID activation, rollback, state
   continuity, and journal ownership.
4. **Behavior migration.** Move high-churn logic out of compiled C++ in
   measurable slices. Keep native mechanisms and only the performance-critical
   policy that fails the tick budget.
5. **Coarse native modules.** Split performance-critical mechanisms into 8-16
   modules, remove the shared PCH from module builds, and measure one-module
   latency separately.
6. **Cutover and removal.** Delete the duplicate resident/hot paths, enforce the
   ownership matrix, and record the accepted p95 and runtime performance.

Each gate reports source, benchmark, runtime, and human acceptance separately.
A successful load or a passing synthetic benchmark is not proof of visible
behavior.

## Acceptance evidence

- 50 changed hot files: p95 save-to-active <=1.000 s on the reference laptop,
  with the complete stage breakdown and repeated runs.
- Runtime performance: representative NPC/combat/network workloads remain within
  the approved tick budget; state whether the hot layer is faster, equal, or
  slower than the C++ baseline.
- Journal evidence: the real executable shows the hot path and owner event for
  the exercised behavior, with no resident fallback.
- Rollback: an unsupported or failing candidate leaves the prior generation
  active and recorded.
- Native module change: separate measured compile/link/activate result, with no
  claim that it meets one second.
- No per-file DLL and no claim that arbitrary 50 C++ TUs meet one second.

## Non-goals and non-promises

- This is not a Cutover-1 change and does not relax any Cutover-1 requirement.
- It does not make arbitrary C++ translation units compile or link in one second.
- It does not authorize per-file DLLs.
- It does not authorize agents to run builds or compilers.
- It does not replace live journal evidence, runtime proof, or human acceptance.
- It does not select C#, C++ JIT, AngelScript, or LuaJIT before the bake-off.

## Reference evidence (2026-11-19)

The measurements above are dated reference evidence, not current status. The
whole-game change set, hardware, cache state, and source counts must be
regenerated before using them as acceptance evidence. The current warm no-op and
changed-build behavior remains owned by
[`../dev-loop/build-time-observability.md`](../dev-loop/build-time-observability.md).
