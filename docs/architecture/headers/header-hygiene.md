# Header hygiene and fan-out budget

Status: active architecture rule; NPC/FCS tranche in progress, broader split
tracked by `issue-196c3045b4cb`.

## Purpose

Reduce edit-to-evidence time by making a header change rebuild as few
translation units as practical. Hot game-DLL reload is one accelerator; this
rule also covers config/data reload, smaller translation units, warm runtimes,
and offline journal analysis.

A successful build is not behavior proof. Header work is compile-latency work
and must stay behavior-neutral unless a separate behavior proof exists.

## Measure first

Use the compiler-generated dependency graph, not intuition:

```text
python devscripts/dependency_graph.py --top 50
python devscripts/dependency_graph.py --affected <header>
python devscripts/dependency_graph.py --dependents <header>
```

Record the affected-source count in the build note for any header edit.

## Baseline (2026-11-23)

Direct fan-in from `devscripts/dependency_graph.py`:

| Header | Fan-in | Classification | First action |
|---|---:|---|---|
| `src/pch.h` | 456 | Precompiled header, third-party only | Keep stable; never add project headers. |
| `src/debug/structured-log.h` | 355 | Resident logging interface | One-time cold split after NPC tranche; ABI review. |
| `src/physics/physics-types.h` | 313 | Physics/collision vocabulary | Split by concept (later tranche). |
| `src/physics/movement/movement-types.h` | 293 | Movement vocabulary | Split command/state/config/contact. |
| `src/combat/weapon-types.h` | 287 | Weapon definition/runtime | Split ids/definition/runtime/recoil. |
| `src/vip/vip-appearance.h` | 287 | Cosmetic types | Remove from broad aggregates. |
| `src/map/map-mesh-types.h` | 285 | Map/collision/render types | Split collision from render mesh types. |
| `src/config.h` | 284 | Global debug flags | Split by domain; NPC flags first. |
| `src/entities/avatar-rig.h` | 281 | Rig/capability types | Split identity from render/movement data. |
| `src/debug/debug-log.h` | 276 | Debug macros | Keep thin; split formatting if needed. |
| `src/entities/actor-body.h` | 271 | Actor body state | Split body/collision/animation. |
| `src/avatar/avatar-autosave.h` | 255 | Autosave types | Keep out of generic actor headers. |
| `src/avatar/avatar.h` | 255 | Avatar types | Include only in avatar owners. |
| `src/gui/hud/chat-bubble.h` | 244 | HUD type | Remove from non-HUD includes. |
| `src/combat/weapon-collision-config.h` | 243 | Collision config | Split from generic weapon types. |
| `src/entities/player.h` | 243 | Player aggregate (958 lines) | Hybrid split after NPC tranche. |
| `src/physics/movement/actor-final-pose.h` | 243 | Final-pose state/solver | Split state from solver interface. |
| `src/physics/config.h` | 228 | Global dimensions/movement constants | Extract `player-dimensions.h`. |
| `src/map/map_common.h` | 210 | Map common | Narrow includes. |
| `src/network/packets.h` | 187 | Packet aggregate | Per-family schema includes only. |
| packet schema headers | 182 each | Network schema aggregates | Include only in network owners. |
| `src/host/abi/game-host-abi.h` | 180 | Cold ABI | Change only with EXE/ABI review. |
| `src/npc/npc.h` | 143 | NPC aggregate (464 lines) | First active tranche. |
| `src/npc/npc-utility.h` | 147 | Utility types/actions | Split pure types from executor. |
| `src/npc/npc-behavior.h` | 145 | Behavior tuning | Split types from profile execution. |

Observed build cost: changing `src/npc/npc.h` affected 143 sources and the
gen568 whole-game build took 1,756.786 s across 642 sources.

## Header classes

Every header states its purpose, owner, and one class:

- **POD/type header** — no behavior, no heavy includes, safe to include widely.
- **Owner interface** — declares one owner; forward-declares other owners.
- **Implementation detail** — included only by that owner's `.cpp` or a narrow
  detail header.
- **Cold ABI** — `src/host/abi/**`; changing it requires EXE/ABI review.
- **PCH** — third-party and system only; no project headers.

## Rules

1. One concept per header. Do not append unrelated types to a common header.
2. Public headers forward-declare unless the interface needs the complete type.
3. Include only what the declaration needs; move includes to the `.cpp`.
4. Include direction is one-way: `*-types.h` → owner interface → orchestration.
   Never include a higher layer from a lower layer.
5. No project headers in `src/pch.h`.
6. A new cross-owner call uses a narrow interface header with plain data, not a
   full mutable owner class.
7. Prefer composition of small state sub-structs over one large mutable class.
   Hot simulation fields stay by value; cold animation/network/render/cosmetic
   state may move behind pointers or separate headers (hybrid boundary).
8. Header-only changes are behavior-neutral. Do not mix them with gameplay
   changes in the same build.
9. Split large translation units along owners when practical so one edit
   compiles less.
10. Prefer JSON/config/data for tuning; compile only for structural behavior.

## Fan-out budget

For a changed header, use `dependency_graph.py --affected`:

| Affected sources | Requirement |
|---:|---|
| 0–20 | Normal header edit. |
| 21–50 | State the count in the build note and keep the change narrow. |
| 51–100 | Write a short fanout plan in the issue/build note before editing. |
| >100 | Human/root review before editing; prefer splitting first. |

No new common header above 100 affected sources. Existing high-fanout headers
are reduced in tranches, not by adding more to them.

## Workflow

1. Run `dependency_graph.py --affected <header>`.
2. If over budget, split into type/interface/detail headers before adding.
3. Use an edit lease for each header.
4. Build header-only changes separately from behavior changes; request one
   batch build through the root dev-loop after the split is coherent.
5. Re-run the dependency graph and record the before/after affected counts and
   build time.
6. Update the feature record or issue with the measurement.

## Tooling

- `devscripts/dependency_graph.py` — report-only fan-in and affected sets.
- `devscripts/cold_change_guard.py` — `HEADER_REVIEW`, `COLD_ABI`, and owner
  classification before a build.
- `devscripts/build-note.py` — include the affected-source count for header
  edits.
- Planned `header_budget_check.py` — report-only first, then warn/fail on
  changed headers over budget.

## Tranches

1. **NPC/FCS bubble (current):** `src/npc/**`, `src/entities/player.h`,
   `src/entities/actor-body.h`, `src/physics/movement/actor-final-pose.h`,
   `src/physics/config.h`, `src/config.h` NPC debug flags.
2. **Physics/movement/weapon/map/avatar types.**
3. **Network packet aggregates.**
4. **Cold headers** (`structured-log.h`, `game-host-abi.h`) only with a measured
   reason and EXE/ABI review.

Broad tracking issue: `issue-196c3045b4cb`.

## Acceptance

- `src/npc/npc.h` affected sources: 143 → target <50.
- `src/entities/player.h`: 243 → target <80.
- A single NPC debug flag or player-dimension change: <20 affected sources.
- One narrow header edit: target <60 s build.
- Before/after dependency-graph and real build-time evidence recorded.
- No behavior change in a header-only build.
